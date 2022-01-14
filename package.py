# -*- coding: utf-8 -*-#

# YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
# Copyright © 2004-2022 YaPB Project <yapb@jeefo.net>.
#
# SPDX-License-Identifier: MIT
# 

import os, sys, subprocess, base64
import urllib3
import pathlib, shutil
import zipfile, tarfile
import datetime, calendar

class CodeSign (object):
   def __init__(self, product, url, verify_signature=False):
      self.signing = True

      self.ossl_path = "/usr/bin/osslsigncode"
      self.local_key = os.path.join (pathlib.Path ().absolute (), "key.pfx");

      self.product = product
      self.url = url
      self.verify_signature = verify_signature

      if not os.path.exists (self.ossl_path):
         self.signing = False

      if not "CS_CERTIFICATE" in os.environ:
         self.signing = False

      if not "CS_CERTIFICATE_PASSWORD" in os.environ:
         self.signing = False

      if self.signing:
         self.password = os.environ.get ("CS_CERTIFICATE_PASSWORD")

         encoded = os.environ.get ("CS_CERTIFICATE")

         if len (encoded) < 64:
            print ('Damaged certificate. Signing disabled.')
            self.signing = False
            return

         decoded = base64.b64decode (encoded)

         with open (self.local_key, "wb") as key:
            key.write (decoded)

   def has(self):
      return self.signing

   def sign_file_inplace (self, filename):
      signed_filename = filename + ".signed"
      sign = []

      sign.append (self.ossl_path)
      sign.append ("sign")
      sign.append ("-pkcs12")
      sign.append (self.local_key)
      sign.append ("-pass")
      sign.append (self.password)
      sign.append ("-n")
      sign.append (self.product)
      sign.append ("-i")
      sign.append (self.url)
      sign.append ("-h")
      sign.append ("sha384")
      sign.append ("-t")
      sign.append ("http://timestamp.sectigo.com")
      sign.append ("-in")
      sign.append (filename)
      sign.append ("-out")
      sign.append (signed_filename)

      result = subprocess.run (sign, capture_output=True, text=True)

      if result.returncode == 0:
         os.unlink (filename)
         shutil.move (signed_filename, filename)

         print ("Signing result: {}".format (result.stdout))

         if self.verify_signature:
            verify = []
            verify.append (self.ossl_path)
            verify.append ("verify")
            verify.append (filename)

            verify = subprocess.run (verify, capture_output=True, text=True)
            print (verify.stdout)

      else:
         print (result.stdout)


class BotRelease (object):
   def __init__(self):
      print ("Initializing Packaging")

      meson_src_root_env = "MESON_SOURCE_ROOT"

      if meson_src_root_env in os.environ:
         os.chdir (os.environ.get (meson_src_root_env))
      
      self.work_dir = os.path.join (pathlib.Path ().absolute (), "cfg")
      self.bot_dir = os.path.join (self.work_dir, "addons", "yapb")
      self.pkg_dir = os.path.join (pathlib.Path ().absolute (), "pkg")
      
      if len (sys.argv) < 2:
         raise Exception("Missing required parameters.")

      self.version = sys.argv[1]
      self.artifacts = 'artifacts'

      self.cs = CodeSign ("YaPB", "https://yapb.jeefo.net/")

      if self.cs.has ():
         print ("Code Signing Enabled")
      else:
         print ("Code Signing Disabled")

      os.makedirs (self.pkg_dir, exist_ok=True)
      
      self.pkg_win32 = os.path.join (self.pkg_dir, "yapb-{}-windows.zip".format (self.version))
      self.pkg_linux = os.path.join (self.pkg_dir, "yapb-{}-linux.tar.xz".format (self.version))
      self.pkg_macos = os.path.join (self.pkg_dir, "yapb-{}-macos.zip".format (self.version))

      self.pkg_win32_sfx = self.pkg_win32.replace ("zip", "exe")
      self.pkg_win32_sfx_url = "https://github.com/yapb/setup/releases/latest/download/botsetup.exe"
      
   def make_directories (self):
      dirs = [
         "bin",
         os.path.join ("data", "pwf"),
         os.path.join ("data", "train"),
         os.path.join ("data", "graph"),
         os.path.join ("data", "logs")
      ]
      
      for dir in dirs:
         os.makedirs (os.path.join (self.bot_dir, dir), exist_ok=True)
   
   def http_pull (self, url, local_file):
      http = urllib3.PoolManager (10, headers = {"user-agent": "YaPB"})
      data = http.urlopen ("GET", url)
      
      with open (local_file, "wb") as file:
         file.write (data.data)
         
   def get_graph_file (self, name):
      file = os.path.join (self.bot_dir, "data", "graph", "{}.graph".format (name))
      url = "http://yapb.jeefo.net/graph/{}.graph".format (name)
      
      if os.path.exists (file):
         return
         
      self.http_pull (url, file)

   def get_default_graphs (self):
      print ("Downloading graphs: ")
      
      default_list = "default.graph.txt"
      self.http_pull ("http://graph.yapb.ru/DEFAULT.txt", default_list)

      with open (default_list) as file:
         files = [line.rstrip () for line in file.readlines ()]
      
      for file in files:
         print (" " + file)
         
         self.get_graph_file (file)

   def unlink_binaries (self):
      libs = ["yapb.so", "yapb.arm64.so", "yapb.dll", "yapb.dylib"]
      
      for lib in libs:
         path = os.path.join (self.bot_dir, "bin", lib)
         
         if os.path.exists (path):
            os.remove (path)

   def sign_binary (self, binary):
      if self.cs.has () and (binary.endswith ("dll") or binary.endswith ("exe")):
         print ("Signing {}".format (binary))
         self.cs.sign_file_inplace (binary)

   def copy_binary (self, binary):
      dest_path = os.path.join (self.bot_dir, "bin", os.path.basename (binary))
      shutil.copy (binary, dest_path)
      
      self.sign_binary (dest_path)

   def compress_directory (self, path, handle):
      length = len (path) + 1
      empty_dirs = []
      
      for root, dirs, files in os.walk (path):
         empty_dirs.extend ([dir for dir in dirs if os.listdir (os.path.join (root, dir)) == []]) 
         
         for file in files:
            file_path = os.path.join (root, file)
            handle.write (file_path, file_path[length:])
            
         for dir in empty_dirs:
            dir_path = os.path.join (root, dir)
            
            zif = zipfile.ZipInfo (dir_path[length:] + "/")
            handle.writestr (zif, "")
            
         empty_dirs = []

   def create_zip (self, dir):
      zf = zipfile.ZipFile (dir, "w", zipfile.ZIP_DEFLATED, compresslevel=9)
      zf.comment = bytes (self.version, encoding = "ascii")
      
      self.compress_directory (self.work_dir, zf)
      zf.close ()
   
   def convert_zip_txz (self, zfn, txz):
      timeshift = int ((datetime.datetime.now () - datetime.datetime.utcnow ()).total_seconds ())
      
      with zipfile.ZipFile (zfn) as zipf:
         with tarfile.open (txz, "w:xz") as tarf:
            for zif in zipf.infolist ():
               tif = tarfile.TarInfo (name = zif.filename)
               tif.size = zif.file_size
               tif.mtime =  calendar.timegm (zif.date_time) - timeshift
               
               tarf.addfile (tarinfo = tif, fileobj = zipf.open (zif.filename))
               
      os.remove (zfn)

   def install_binary (self, ext, unlink_existing = True):
      lib = "yapb.{}".format (ext)
      binary = os.path.join (self.artifacts, lib)

      if os.path.isdir (binary):
         binary = os.path.join (binary, lib)
      
      if not os.path.exists (binary):
         print ("Packaging failed for {}. Skipping...".format (lib))
         return False
         
      if unlink_existing:
         self.unlink_binaries ()

      self.copy_binary (binary)

      return True
      
   def create_pkg_win32 (self):
      print ("Generating Win32 ZIP")
      
      if not self.install_binary ("dll"):
         return

      self.create_zip (self.pkg_win32)
      self.http_pull (self.pkg_win32_sfx_url, "botsetup.exe")
      
      print ("Generating Win32 EXE")

      with open ("botsetup.exe", "rb") as sfx, open (self.pkg_win32, "rb") as zfn, open (self.pkg_win32_sfx, "wb") as exe:
         exe.write (sfx.read ())
         exe.write (zfn.read ())

      self.sign_binary (self.pkg_win32_sfx)
      
   def create_pkg_linux (self):
      print ("Generating Linux TXZ")

      self.unlink_binaries ()
      self.install_binary ("arm64.so")

      if not self.install_binary ("so", False):
         return
      
      tmp_file = "tmp.zip"
      
      self.create_zip (tmp_file)
      self.convert_zip_txz (tmp_file, self.pkg_linux)

   def create_pkg_macos (self):
      print ("Generating macOS ZIP")
      
      if not self.install_binary ("dylib"):
         return
      
      self.create_zip (self.pkg_macos)
      
   def create_pkgs (self):
      self.create_pkg_linux ()
      self.create_pkg_win32 ()
      self.create_pkg_macos ()

release = BotRelease ()

release.make_directories ()
release.get_default_graphs ()
release.create_pkgs ()

