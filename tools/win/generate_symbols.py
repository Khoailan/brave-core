#!/usr/bin/env python3
# Copyright (c) 2023 The Brave Authors. All rights reserved.
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at https://mozilla.org/MPL/2.0/.
"""Creates symsrv directory structure containing exe/dll/pdb files."""

import configparser
import errno
import argparse
import os
import re
import sys
import subprocess

from datetime import datetime
from shutil import rmtree, copy

ROOT_DIR = os.path.join(os.path.dirname(__file__), *[os.pardir] * 3)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--installer-config',
        required=True,
        help='Installer config that contains files to process.',
    )
    parser.add_argument(
        '--additional-files',
        nargs='+',
        help='Additional exe/dll files to process.',
    )
    parser.add_argument(
        '--build-dir',
        required=True,
        help='The build directory.',
    )
    parser.add_argument(
        '--toolchain-dir',
        required=True,
        help='The toolchain directory.',
    )
    parser.add_argument(
        '--symbols-dir',
        required=True,
        help='Directory to output files for symbols server (exe, dll, pdb).',
    )
    parser.add_argument(
        '--pdb-only-symbols-dir',
        help='Directory to output only pdb files.',
    )
    parser.add_argument(
        '--run-source-index',
        default=False,
        action='store_true',
        help='Run source_index.py from the Chromium toolset.',
    )
    parser.add_argument(
        '--clear',
        default=False,
        action='store_true',
        help='Clear the symbol directories before writing new symbols.',
    )
    parser.add_argument(
        '-v',
        '--verbose',
        action='store_true',
        help='Print verbose status output.',
    )

    args = parser.parse_intermixed_args()

    if args.clear:
        try:
            rmtree(args.symbols_dir)
            if args.pdb_only_symbols_dir:
                rmtree(args.pdb_only_symbols_dir)
        except:  # pylint: disable=bare-except
            pass

    images_with_pdbs = get_images_with_pdbs(args)
    for image_path in images_with_pdbs:
        process_image(args, image_path)


def get_images_with_pdbs(args):
    images_with_pdbs = set()

    def add_if_has_pdb(file, should_exist=False):
        file = os.path.join(args.build_dir, file)
        if not os.path.exists(file):
            if should_exist:
                raise RuntimeError(f'{file} not found')
            return
        pdb_file = file + '.pdb'
        if not os.path.exists(pdb_file):
            return
        images_with_pdbs.add(file)

    installer_config = configparser.ConfigParser()
    installer_config.optionxform = str  # Preserve string case.
    installer_config.read(args.installer_config)
    for group in installer_config:
        if group == 'GOOGLE_CHROME':
            # Skip Google Chrome-only files.
            continue

        for file in installer_config[group]:
            add_if_has_pdb(file)

    if args.additional_files:
        for file in args.additional_files:
            add_if_has_pdb(file, True)

    return [*images_with_pdbs]


def process_image(args, image_path):
    assert os.path.isabs(image_path)
    start_time = datetime.utcnow()
    if args.verbose:
        print(f'Processing {image_path}')

    image_pdb_fingerprint, pdb_path = get_pdb_info_from_img(image_path)
    if args.verbose:
        print(f'  img fingerprint: {image_pdb_fingerprint}')

    assert os.path.isabs(pdb_path)
    pdb_fingerprint = get_pdb_fingerprint(pdb_path)
    if args.verbose:
        print(f'  pdb fingerprint: {pdb_fingerprint}')

    if pdb_fingerprint != image_pdb_fingerprint:
        raise ValueError(
            f"Image PDB fingerprint doesn't match PDB fingerprint:\n"
            f"  {image_pdb_fingerprint} : {image_path}\n"
            f"  {pdb_fingerprint} : {pdb_path}")

    copy_symbol(image_path, image_pdb_fingerprint, args.symbols_dir)
    copied_pdb_path = copy_symbol(pdb_path, pdb_fingerprint, args.symbols_dir)

    # Inject source server information into PDBs to allow VS/WinDBG
    # automatically fetch sources from GitHub.
    #
    # Note: this will modify PDB header age, but not DBI age which allows
    # debuggers to properly work with the modified PDB. More info in comments:
    # https://randomascii.wordpress.com/2011/11/11/source-indexing-is-underused-awesomeness/
    if args.run_source_index:
        source_index_env = {
            **os.environ,
            # The file list output of `pdbstr.exe` is put into set(). Disable
            # hash randomization to keep the file order stable between runs.
            'PYTHONHASHSEED': '0',
        }
        subprocess.check_call([
            'vpython3.bat',
            os.path.join(ROOT_DIR, 'tools', 'symsrc', 'source_index.py'),
            '--build-dir',
            args.build_dir,
            '--toolchain-dir',
            args.toolchain_dir,
            copied_pdb_path,
        ],
                              env=source_index_env)

    if args.pdb_only_symbols_dir:
        copy_symbol(copied_pdb_path, pdb_fingerprint, args.pdb_only_symbols_dir)

    if args.verbose:
        elapsed = datetime.utcnow() - start_time
        print(f'Copied symbols for {image_path}: elapsed time '
              f'{elapsed.total_seconds()} seconds')


def get_pdb_info_from_img(image_path):
    if 'GetPDBInfoFromImg' not in globals():
        sys.path.append(os.path.join(ROOT_DIR, 'tools', 'symsrc'))

        # pylint: disable=import-error,import-outside-toplevel
        from pdb_fingerprint_from_img import GetPDBInfoFromImg

    return GetPDBInfoFromImg(image_path)


def get_pdb_fingerprint(pdb_path):
    assert os.path.isabs(pdb_path)
    llvm_pdbutil_path = os.path.join(ROOT_DIR, 'third_party', 'llvm-build',
                                     'Release+Asserts', 'bin',
                                     'llvm-pdbutil.exe')

    stdout = subprocess.check_output(
        [llvm_pdbutil_path, 'dump', '--summary', pdb_path], encoding='oem')

    age_pattern = r'  Age: (.*)'
    guid_pattern = r'  GUID: {(.*)}'

    age_match = None
    guid_match = None
    for line in stdout.splitlines():
        if not age_match:
            age_match = re.match(age_pattern, line)
        if not guid_match:
            guid_match = re.match(guid_pattern, line)

    if not age_match or not guid_match:
        raise RuntimeError(
            f'PDB info not matched in llvm-pdbutil output for {pdb_path}:\n'
            f'{stdout}')

    return guid_match.group(1).replace('-', '') + age_match.group(1)


def copy_symbol(symbol_path, symbol_fingerprint, dest_symbols_dir):
    symbol_name = os.path.basename(symbol_path)
    dest_symbol_dir = os.path.join(dest_symbols_dir, symbol_name,
                                   symbol_fingerprint)
    mkdir_p(dest_symbol_dir)
    dest_symbol_path = os.path.join(dest_symbol_dir, symbol_name)
    copy(symbol_path, dest_symbol_path)
    return dest_symbol_path


def mkdir_p(path):
    '''Simulates mkdir -p.'''
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


if __name__ == '__main__':
    main()
