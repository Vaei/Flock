# Copyright (c) Jared Taylor. All Rights Reserved
"""Checks the documentation site. Standard library only, no build step.

    python docs/check_docs.py            report
    python docs/check_docs.py --wanted   just the missing art, as a markdown table

Reports dead internal links, figures placed but not declared, figures declared but
never placed, a figure whose `page` disagrees with where it is used, and every
declared file that is not in img/ yet.
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, 'assets')
IMG = os.path.join(HERE, 'img')


def read(path):
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()


def parse_site():
    src = read(os.path.join(ASSETS, 'site.js'))
    head, _, shots_src = src.partition('shots:')

    pages = []
    for m in re.finditer(r"file:\s*'([^']+)'\s*,\s*label:\s*'([^']+)'", head):
        pages.append((m.group(1), m.group(2)))

    shots = {}
    for m in re.finditer(r"'([A-Za-z0-9_.\-]+)':\s*\{(.*?)\}\s*(?=,\s*'|\s*\}\s*\};)", shots_src, re.S):
        body = m.group(2)
        entry = {}
        for key in ('page', 'cap', 'file', 'compare', 'vid', 'yt', 'poster'):
            k = re.search(key + r":\s*'([^']*)'", body)
            if k:
                entry[key] = k.group(1)
        shots[m.group(1)] = entry
    return pages, shots


def parse_pages():
    """id -> set of pages placing it, plus every internal href per page."""
    placed = {}
    links = {}
    for name in sorted(os.listdir(HERE)):
        if not name.endswith('.html'):
            continue
        src = read(os.path.join(HERE, name))
        ids = re.findall(r'data-shot="([^"]+)"', src)
        for group in re.findall(r'data-shots="([^"]+)"', src):
            ids += [s.strip() for s in group.split(',') if s.strip()]
        for i in ids:
            placed.setdefault(i.strip(), set()).add(name)
        links[name] = [h for h in re.findall(r'href="([^"]+)"', src)
                       if not h.startswith(('http', '#', 'mailto:'))]
    return placed, links


def main():
    pages, shots = parse_site()
    placed, links = parse_pages()
    problems = []
    wanted = []

    listed = {p for p, _ in pages}
    on_disk = {n for n in os.listdir(HERE) if n.endswith('.html')}
    for p in sorted(listed - on_disk):
        problems.append('nav lists %s, which does not exist' % p)
    for p in sorted(on_disk - listed):
        problems.append('%s exists but is not in the nav' % p)

    for page, hrefs in sorted(links.items()):
        for h in hrefs:
            target = os.path.normpath(os.path.join(HERE, h.split('#')[0]))
            if not os.path.exists(target):
                problems.append('%s links to %s, which does not exist' % (page, h))

    for i in sorted(placed):
        if i not in shots:
            problems.append('%s places undeclared figure %s' % (', '.join(sorted(placed[i])), i))

    for i, s in sorted(shots.items()):
        where = placed.get(i)
        if not where:
            problems.append('figure %s is declared and never placed' % i)
        elif s.get('page') and s['page'] not in where:
            problems.append('figure %s says page %s but is placed on %s'
                            % (i, s['page'], ', '.join(sorted(where))))
        for key in ('file', 'compare', 'vid', 'poster'):
            f = s.get(key)
            if f and not os.path.exists(os.path.join(IMG, f)):
                wanted.append((f, i, s.get('page', '?'), s.get('cap', '')))

    if '--wanted' in sys.argv:
        print('| file | figure | page | what it shows |')
        print('| --- | --- | --- | --- |')
        for f, i, p, cap in sorted(wanted):
            print('| `%s` | `%s` | %s | %s |' % (f, i, p, cap))
        return 0

    for line in problems:
        print('problem: ' + line)
    print('')
    print('%d pages, %d figures, %d files wanted, %d problems'
          % (len(pages), len(shots), len(wanted), len(problems)))
    if wanted:
        print('')
        print('wanted art (--wanted for a table):')
        for f, i, p, _ in sorted(wanted)[:40]:
            print('  %-44s %s' % (f, i))
        if len(wanted) > 40:
            print('  ... and %d more' % (len(wanted) - 40))
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
