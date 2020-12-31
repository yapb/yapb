# -*- coding: utf-8 -*-#

# YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
# Copyright © 2004-2021 YaPB Project <yapb@jeefo.net>.
#
# SPDX-License-Identifier: MIT
# 

import os, sys, subprocess
import locale, urllib3
import pathlib, shutil
import zipfile, tarfile
import datetime, calendar

from github import Github

class BotRelease (object):
   def __init__(self):
      print ("Initializing Packaging")
      
      self.workDir = os.path.join (pathlib.Path ().absolute (), 'cfg')
      self.botDir = os.path.join (self.workDir, 'addons', 'yapb')
      self.distDir = os.path.join (pathlib.Path ().absolute (), 'dist');
      
      if len (sys.argv) < 2:
         raise Exception('Missing required parameters.')

      self.version = sys.argv[1]
      
      os.makedirs (self.distDir, exist_ok=True)
      
      self.outFileWin32 = os.path.join (self.distDir, "yapb-{}-windows.zip".format (self.version))
      self.outFileLinux = os.path.join (self.distDir, "yapb-{}-linux.tar.xz".format (self.version))
      self.outFileMacOS = os.path.join (self.distDir, "yapb-{}-macos.zip".format (self.version))
      
      self.outFileWin32Setup = self.outFileWin32.replace ("zip", "exe")
      self.win32SetupUrl = "https://github.com/yapb/setup/releases/latest/download/botsetup.exe"
      
   def makeDirs (self):
      dirs = [
         'bin', 
         os.path.join ('data', 'pwf'),
         os.path.join ('data', 'train'),
         os.path.join ('data', 'graph'),
         os.path.join ('data', 'logs')
      ]
      
      for dir in dirs:
         os.makedirs (os.path.join (self.botDir, dir), exist_ok=True)
   
   def download (self, fromWhere, toFile):
      http = urllib3.PoolManager (10, headers = {"user-agent": "YaPB"})
      data = http.urlopen ('GET', fromWhere)
      
      with open (toFile, "wb") as file:
         file.write (data.data)
         
   def getGraph (self, name):
      file = os.path.join (self.botDir, 'data', 'graph', "{}.graph".format (name))
      url = "http://graph.yapb.ru/graph/{}.graph".format (name);
      
      if os.path.exists (file):
         return
         
      self.download (url, file)

   def getGraphs (self):
      print ("Downloading graphs: ")
      
      files = ['as_oilrig', 'cs_747', 'cs_estate', 'cs_assault', 'cs_office',
         'cs_italy', 'cs_havana', 'cs_siege', 'cs_backalley', 'cs_militia',
         'cs_downed_cz', 'cs_havana_cz', 'cs_italy_cz', 'cs_militia_cz',
         'cs_office_cz', 'de_airstrip_cz', 'de_aztec_cz', 'de_cbble_cz',
         'de_chateau_cz', 'de_corruption_cz', 'de_dust_cz', 'de_dust2_cz',
         'de_fastline_cz', 'de_inferno_cz', 'de_piranesi_cz', 'de_prodigy_cz',
         'de_sienna_cz', 'de_stadium_cz', 'de_tides_cz', 'de_torn_cz',
         'de_truth_cz', 'de_vostok_cz', 'de_inferno', 'de_aztec', 'de_dust',
         'de_dust2', 'de_torn', 'de_storm', 'de_airstrip', 'de_piranesi',
         'de_prodigy', 'de_chateau', 'de_cbble', 'de_nuke', 'de_survivor',
         'de_vertigo', 'de_train']
      
      for file in files:
         print (" " + file)
         
         self.getGraph (file)

   def unlinkBinaries (self):
      libs = ['yapb.so', 'yapb.dll', 'yapb.dylib']
      
      for lib in libs:
         path = os.path.join (self.botDir, 'bin', lib)
         
         if os.path.exists (path):
            os.remove (path)
   
   def copyBinary (self, path):
      shutil.copy (path, os.path.join (self.botDir, 'bin'))
      
   def zipDir (self, path, handle):
      length = len (path) + 1
      emptyDirs = []  
      
      for root, dirs, files in os.walk (path):
         emptyDirs.extend ([dir for dir in dirs if os.listdir (os.path.join (root, dir)) == []])  
         
         for file in files:
            filePath = os.path.join (root, file)
            handle.write (filePath, filePath[length:])
            
         for dir in emptyDirs:  
            dirPath = os.path.join (root, dir)
            
            zif = zipfile.ZipInfo (dirPath[length:] + "/")  
            handle.writestr (zif, "")  
            
         emptyDirs = []

   def generateZip (self, dir): 
      zipFile = zipfile.ZipFile (dir, 'w', zipfile.ZIP_DEFLATED)
      zipFile.comment = bytes (self.version, encoding = "ascii")
      
      self.zipDir (self.workDir, zipFile)
      zipFile.close ()
   
   def convertZipToTXZ (self, zip, txz):
      timeshift = int ((datetime.datetime.now () - datetime.datetime.utcnow ()).total_seconds ())
      
      with zipfile.ZipFile (zip) as zipf:
         with tarfile.open (txz, 'w:xz') as tarf:
            for zif in zipf.infolist ():
               tif = tarfile.TarInfo (name = zif.filename)
               tif.size = zif.file_size
               tif.mtime =  calendar.timegm (zif.date_time) - timeshift
               
               tarf.addfile (tarinfo = tif, fileobj = zipf.open (zif.filename))
               
      os.remove (zip)
      
   def generateWin32 (self):
      print ("Generating Win32 ZIP")
      
      binary = os.path.join ('build_x86_win32', 'yapb.dll')
      
      if not os.path.exists (binary):
         return
         
      self.unlinkBinaries ()
      
      self.copyBinary (binary)
      self.generateZip (self.outFileWin32)
      
      self.download (self.win32SetupUrl, "botsetup.exe")
      
      print ("Generating Win32 EXE")

      with open ("botsetup.exe", "rb") as sfx, open (self.outFileWin32, "rb") as zip, open (self.outFileWin32Setup, "wb") as exe:
         exe.write (sfx.read ())
         exe.write (zip.read ())
      
   def generateLinux (self):
      print ("Generating Linux TXZ")
      
      binary = os.path.join ('build_x86_linux', 'yapb.so');
      
      if not os.path.exists (binary):
         return
         
      self.unlinkBinaries ()
      self.copyBinary (binary)
      
      tmpFile = "tmp.zip"
      
      self.generateZip (tmpFile)
      self.convertZipToTXZ (tmpFile, self.outFileLinux)
      
   def generateMac (self):
      print ("Generating macOS ZIP")
      
      binary = os.path.join ('build_x86_macos', 'yapb.dylib')
      
      if not os.path.exists (binary):
         return
         
      self.unlinkBinaries ()
      self.copyBinary (binary)
      
      self.generateZip (self.outFileMacOS)
      
   def generate (self):
      self.generateWin32 ()
      self.generateLinux ()
      self.generateMac ()
      
   def createRelease (self, repository, version):
      print ("Creating Github Tag")
      
      releases = [self.outFileLinux, self.outFileWin32, self.outFileMacOS, self.outFileWin32Setup]
      
      for release in releases:
         if not os.path.exists (release):
            return
      
      releaseName = "YaPB " + version
      releaseMessage = repository.get_commits()[0].commit.message
      releaseSha = repository.get_commits()[0].sha;

      ghr = repository.create_git_tag_and_release (tag = version, tag_message = version, release_name = releaseName, release_message = releaseMessage, type='commit', object = releaseSha, draft = False)
      
      print ("Uploading packages to Github")
      
      for release in releases:
         ghr.upload_asset( path = release, label = os.path.basename (release)) 
            
   def uploadGithub (self): 
      gh = Github (os.environ["GITHUB_TOKEN"])
      repo = gh.get_repo ("yapb/yapb")
      
      self.createRelease (repo, self.version)
         
release = BotRelease ()

release.makeDirs ()
release.getGraphs ()
release.generate ()
release.uploadGithub ()
