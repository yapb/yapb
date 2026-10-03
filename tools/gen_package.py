# -*- coding: utf-8 -*-#

#
# YaPB, based on PODBot by Count Floyd
# Maintained by YaPB Team <yapb@jeefo.net>
#
# SPDX-License-Identifier: Unlicense
#

import os
import sys
import time
import base64
import shutil
import zipfile
import tarfile
import pathlib
import requests
import subprocess
from dataclasses import dataclass, field

# archive entry timestamps. fixed by default via SOURCE_DATE_EPOCH for
# reproducible packages, otherwise falls back to the current time.
MTIME = int(os.environ.get('SOURCE_DATE_EPOCH', time.time()))


class BotSign(object):
   def __init__(self, product: str, url: str):
      self.signing = False
      self.ossl_path = '/usr/bin/osslsigncode'
      self.local_key = os.path.join(pathlib.Path().absolute(), 'bot_release_key.pfx')

      self.product = product
      self.url = url

      if not os.path.exists(self.ossl_path):
         return

      if 'CS_CERTIFICATE' not in os.environ:
         return

      if 'CS_CERTIFICATE_PASSWORD' not in os.environ:
         return

      self.password = os.environ.get('CS_CERTIFICATE_PASSWORD')
      encoded = os.environ.get('CS_CERTIFICATE')

      if len(encoded) < 64:
         print('Damaged certificate. Signing disabled.')
         return

      with open(self.local_key, 'wb') as key:
         key.write(base64.b64decode(encoded))

      self.signing = True

   def has(self):
      return self.signing

   def sign_file_inplace(self, filename):
      signed_filename = filename + '.signed'
      signed_cmdline = [
         self.ossl_path, 'sign',
         '-pkcs12', self.local_key,
         '-pass', self.password,
         '-n', self.product,
         '-i', self.url,
         '-h', 'sha384',
         '-t', 'http://timestamp.sectigo.com',
         '-in', filename,
         '-out', signed_filename,
      ]

      result = subprocess.run(signed_cmdline, capture_output=True, text=True)

      if result.returncode != 0:
         print(result.stderr)
         return False

      os.unlink(filename)
      shutil.move(signed_filename, filename)

      return True


@dataclass
class BotPackage:
   name: str
   archive: str
   artifact: dict
   extra: bool = False
   amxx: dict = field(default_factory=dict)


class BotRelease(object):
   # binary name suffix per artifact dir suffix, mirroring CMake OUTPUT_NAME
   NAME_SUFFIXES = (
      ('arm64', '_arm64'),
      ('amd64', '_amd64'),
      ('riscv64', '_riscv64d'),
   )

   def __init__(self):
      if len(sys.argv) < 2:
         raise Exception('Missing required parameters.')

      self.project = 'yapb'
      self.version = sys.argv[1]
      self.artifacts = 'artifacts'
      self.graphs = 'https://raw.githubusercontent.com/yapb/graph/master'
      self.win32exe = 'https://github.com/yapb/setup/releases/latest/download/botsetup.exe'

      src_root_env = 'SOURCE_ROOT'

      if src_root_env in os.environ:
         os.chdir(os.environ.get(src_root_env))
      else:
         raise Exception('No direct access, only via cmake build.')

      path = pathlib.Path().absolute()

      if not os.path.isdir(os.path.join(path, self.artifacts)):
         raise Exception('Artifacts directory missing.')

      print(f'Releasing {self.project} v{self.version}')

      self.work_dir = os.path.join(path, 'release')

      # always stage from scratch: merging into a previous release/ breaks
      # copytree reruns (and would silently mix stale files into packages)
      shutil.rmtree(self.work_dir, ignore_errors=True)
      shutil.copytree(f'{path}/cfg', self.work_dir)

      self.bot_dir = os.path.join(self.work_dir, 'addons', self.project)
      self.pkg_dir = os.path.join(path, 'pkg')

      self.cs = BotSign('YaPB', 'https://yapb.jeefo.net/')

      if self.cs.has():
         print('Signing enabled')
      else:
         print('Signing disabled')

      os.makedirs(self.pkg_dir, exist_ok=True)
      self.http_pull(self.win32exe, 'botsetup.exe')

      self.pkg_matrix = [
         BotPackage('windows', 'zip', {'windows-x86': 'dll'}, amxx={'windows-x86': 'yapb_amxx.dll'}),
         BotPackage('windows', 'exe', {'windows-x86': 'dll'}, amxx={'windows-x86': 'yapb_amxx.dll'}),
         BotPackage('linux', 'tar.xz', {'linux-x86': 'so'}, amxx={'linux-x86': 'yapb_amxx_i386.so'}),
         BotPackage('extras', 'zip',
                    {'linux-arm64': 'so',
                     'linux-amd64': 'so',
                     'linux-riscv64': 'so',
                     'linux-x86-nosimd': 'so',
                     'windows-x86-clang': 'dll',
                     'windows-x86-clang-cl': 'dll',
                     'windows-x86-msvc-xp': 'dll',
                     'windows-amd64': 'dll',
                     'apple-amd64': 'dylib',
                     'apple-arm64': 'dylib',
                     }, extra=True),
      ]

   def create_dirs(self):
      for dir in ['pwf', 'train', 'graph', 'logs']:
         os.makedirs(os.path.join(self.bot_dir, 'data', dir), exist_ok=True)

   def http_pull(self, url: str, tp: str):
      with requests.get(url, headers={'User-Agent': 'YaPB/4'}) as r:
         r.raise_for_status()

         with open(tp, 'wb') as f:
            f.write(r.content)

   def get_graph_file(self, name: str):
      file = os.path.join(self.bot_dir, 'data', 'graph', f'{name}.graph')

      if os.path.exists(file):
         return

      self.http_pull(f'{self.graphs}/graph/{name}.graph', file)

   def create_graphs(self):
      default_list = 'default.graph.txt'
      self.http_pull(f'{self.graphs}/DEFAULT.txt', default_list)

      with open(default_list) as file:
         for line in file:
            name = line.rstrip()
            print(f'Getting graphs: {name}       ', end='\r', flush=True)
            self.get_graph_file(name)

      print()

   def create_cacert(self):
      dest = os.path.join(self.work_dir, 'addons', self.project, 'conf', 'extra', 'cacert.pem')
      os.makedirs(os.path.dirname(dest), exist_ok=True)
      self.http_pull('https://curl.se/ca/cacert.pem', dest)

   def zip_tree(self, handle: zipfile.ZipFile, path: str):
      stamp = time.gmtime(MTIME)[:6]

      for root, dirs, files in os.walk(path):
         dirs.sort()
         files.sort()

         for name in dirs:
            full = os.path.join(root, name)

            if not os.listdir(full):
               handle.writestr(zipfile.ZipInfo(os.path.relpath(full, path) + '/', date_time=stamp), '')

         for name in files:
            full = os.path.join(root, name)
            zif = zipfile.ZipInfo(os.path.relpath(full, path), date_time=stamp)
            zif.compress_type = zipfile.ZIP_DEFLATED
            zif.external_attr = (os.stat(full).st_mode & 0xFFFF) << 16

            with open(full, 'rb') as f:
               handle.writestr(zif, f.read())

   def create_zip(self, dest: str, custom_dir: str = None):
      with zipfile.ZipFile(dest, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
         zf.comment = bytes(self.version, encoding='ascii')
         self.zip_tree(zf, custom_dir if custom_dir else self.work_dir)

   def convert_zip_txz(self, zfn: str, txz: str):
      with zipfile.ZipFile(zfn) as zipf:
         with tarfile.open(txz, 'w:xz') as tarf:
            for zif in zipf.infolist():
               tif = tarfile.TarInfo(name=zif.filename)
               tif.size = zif.file_size
               tif.mtime = MTIME

               if zif.is_dir():
                  tif.mode = 0o755

               tarf.addfile(tarinfo=tif, fileobj=zipf.open(zif.filename))

      os.remove(zfn)

   def convert_zip_sfx(self, zfn: str, exe: str):
      with open('botsetup.exe', 'rb') as sfx, open(zfn, 'rb') as arc, open(exe, 'wb') as dest:
         shutil.copyfileobj(sfx, dest)
         shutil.copyfileobj(arc, dest)

      self.sign_binary(exe)

   def unlink_binaries(self):
      path = os.path.join(self.bot_dir, 'bin')

      shutil.rmtree(path, ignore_errors=True)
      os.makedirs(path, exist_ok=True)

   def unlink_amxx(self):
      path = os.path.join(self.work_dir, 'addons', 'amxmodx', 'modules')

      shutil.rmtree(path, ignore_errors=True)

   def sign_binary(self, binary: str):
      if self.cs.has() and (binary.endswith('dll') or binary.endswith('exe')):
         self.cs.sign_file_inplace(binary)

   def copy_binary(self, binary: str, artifact: str):
      if artifact:
         dest_path = os.path.join(self.bot_dir, 'bin', artifact)
         os.makedirs(dest_path, exist_ok=True)
      else:
         dest_path = os.path.join(self.bot_dir, 'bin')

      dest_path = os.path.join(dest_path, os.path.basename(binary))
      shutil.copy(binary, dest_path)
      self.sign_binary(dest_path)

   def binary_name(self, artifact: str):
      for suffix, postfix in self.NAME_SUFFIXES:
         if artifact.endswith(suffix):
            return self.project + postfix

      return self.project

   def install_binary(self, pkg: BotPackage):
      num_artifacts_errors = 0
      num_artifacts = len(pkg.artifact)

      for artifact in pkg.artifact:
         binary = os.path.join(self.artifacts, artifact, f'{self.binary_name(artifact)}.{pkg.artifact[artifact]}')
         binary_base = os.path.basename(binary)

         if not os.path.exists(binary):
            num_artifacts_errors += 1
            print(f'[{binary_base}: FAIL]', end=' ')
            continue

         print(f'[{binary_base}: OK]', end=' ')

         if num_artifacts == 1:
            self.unlink_binaries()

         self.copy_binary(binary, artifact if pkg.extra else None)

      return num_artifacts_errors < num_artifacts

   def install_amxx(self, pkg: BotPackage):
      dest_dir = os.path.join(self.work_dir, 'addons', 'amxmodx', 'modules')
      os.makedirs(dest_dir, exist_ok=True)

      for artifact, filename in pkg.amxx.items():
         binary = os.path.join(self.artifacts, artifact, filename)
         binary_base = os.path.basename(binary)

         if not os.path.exists(binary):
            print(f'[{binary_base}: FAIL]', end=' ')
            continue

         print(f'[{binary_base}: OK]', end=' ')

         dest = os.path.join(dest_dir, binary_base)
         shutil.copy(binary, dest)
         self.sign_binary(dest)

   def create_pkg(self, pkg: BotPackage):
      dest = os.path.join(self.pkg_dir, f'{self.project}-{self.version}-{pkg.name}.{pkg.archive}')
      dest_tmp = f'{dest}.tmp'

      if os.path.exists(dest):
         os.remove(dest)
      self.unlink_binaries()
      self.unlink_amxx()

      print(f'Generating {os.path.basename(dest)}:', end=' ')

      if not self.install_binary(pkg):
         print(' -> Failed...')
         return

      if not pkg.extra:
         self.install_amxx(pkg)

      if dest.endswith('zip') or dest.endswith('exe'):
         if pkg.extra:
            self.create_zip(dest_tmp, os.path.join(self.bot_dir, 'bin'))
         else:
            self.create_zip(dest_tmp)

         if dest.endswith('exe'):
            self.convert_zip_sfx(dest_tmp, dest)
         else:
            shutil.move(dest_tmp, dest)
      elif dest.endswith('tar.xz'):
         self.create_zip(dest_tmp)
         self.convert_zip_txz(dest_tmp, dest)

      print('-> Success...')
      self.unlink_binaries()
      self.unlink_amxx()

      if os.path.exists(dest_tmp):
         os.remove(dest_tmp)

   def create_pkgs(self):
      for pkg in self.pkg_matrix:
         self.create_pkg(pkg)

      print('Finished release')

   @staticmethod
   def run():
      r = BotRelease()

      r.create_dirs()
      r.create_graphs()
      r.create_cacert()
      r.create_pkgs()

# entry point
if __name__ == "__main__":
   BotRelease.run()
