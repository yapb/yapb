#!/usr/bin/env python3
#
# YaPB, started from PODBot by Count Floyd
# Maintained by YaPB Team <yapb@jeefo.net>
#
# SPDX-License-Identifier: Unlicense
#
# Builds markdown release notes from a commit range, grouped by area.
# Used by CI for the rolling continuous build; also handy by hand:
#
#   python3 tools/gen_release_notes.py --range 4.4.957..HEAD --repo yapb/yapb
#

import argparse
import os
import re
import subprocess
import sys

MAX_COMMITS = 400


def normalize_subject(subject):
    text = subject.lower().strip()
    text = re.sub(r'\s*\(#\d+\)\s*$', '', text)
    text = re.sub(r'\s*\[[^\]]*\]\s*$', '', text)
    return text.strip()

GROUPS = (
    ('Bot AI', ('src/',)),
    ('Configs', ('cfg/',)),
    ('Tests', ('tests/',)),
    ('Build', ('CMakeLists.txt', 'CMakePresets.json', 'build.sh', 'cmake/', 'vc/', 'meson')),
    ('Libraries', ('ext/',)),
    ('CI', ('.github/', '.sourcecraft/')),
    ('Tooling', ('tools/',)),
    ('Docs', ('README.md', 'CONTRIBUTING.md', 'LICENSE', 'UNLICENSE')),
)


def run_git(*args):
    result = subprocess.run(
        ['git', *args], capture_output=True, text=True, check=False)

    if result.returncode != 0:
        return ''

    return result.stdout.strip()


def commit_subjects(rev_range):
    log = run_git('log', '--no-merges', '--format=%H|%h|%s', rev_range)

    if not log:
        return []

    commits = []

    for line in log.splitlines():
        full, short, subject = line.split('|', 2)
        commits.append((full, short, subject))

    return commits


def commit_group(sha):
    files = run_git('diff-tree', '--no-commit-id', '--name-only', '-r', '--root', sha)

    for path in files.splitlines():
        for name, prefixes in GROUPS:
            if any(path == p.rstrip('/') or path.startswith(p) for p in prefixes):
                return name

    return 'Other'


def main():
    parser = argparse.ArgumentParser(description='build markdown release notes from a commit range')
    parser.add_argument('--range', required=True, help='git revision range, e.g. 4.4.957..HEAD')
    parser.add_argument('--repo', default=os.environ.get('GITHUB_REPOSITORY', 'yapb/yapb'),
                        help='owner/name for commit links')
    parser.add_argument('--run-url', default=None, help='link to the CI run')
    parser.add_argument('--since', default=None, help='label of the range base for the header')
    parser.add_argument('--fingerprint', default=None, help='gpg fingerprint for the verify footer')
    args = parser.parse_args()

    commits = commit_subjects(args.range)[:MAX_COMMITS + 1]
    truncated = len(commits) > MAX_COMMITS
    commits = commits[:MAX_COMMITS]

    seen = set()
    unique = []

    for commit in commits:
        key = normalize_subject(commit[2])

        if key in seen:
            continue

        seen.add(key)
        unique.append(commit)

    commits = unique

    if not commits:
        print('Rolling build from master.')
        return 0

    grouped = {}
    order = []

    for full, short, subject in commits:
        group = commit_group(full)

        if group not in grouped:
            grouped[group] = []
            order.append(group)

        url = f'https://github.com/{args.repo}/commit/{full}'
        grouped[group].append(f'- {subject} ([{short}]({url}))')

    lines = []

    if args.since:
        lines.append(f'Changes since `{args.since}`.')
        lines.append('')

    for group in order:
        lines.append(f'### {group}')
        lines.append('')
        lines.extend(grouped[group])
        lines.append('')

    if truncated:
        lines.append(f'...and {len(commit_subjects(args.range)) - MAX_COMMITS} more.')
        lines.append('')

    if args.run_url:
        lines.append(f'Built by [CI run]({args.run_url}).')
        lines.append('')

    if args.fingerprint:
        key = args.fingerprint
        lines.append('---')
        lines.append('Assets ship with detached `.asc` GPG signatures. Verify with the '
                     f'[release key](https://keyserver.ubuntu.com/pks/lookup?op=get&search=0x{key}) (`{key}`):')
        lines.append('')
        lines.append('```sh')
        lines.append(f'gpg --import {key}.asc')
        lines.append('gpg --verify some-yapb.zip.asc some-yapb.zip')
        lines.append('```')

    sys.stdout.write('\n'.join(lines).rstrip() + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
