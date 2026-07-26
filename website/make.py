#!/usr/bin/env python3
import os, sys, subprocess, re, shutil, asyncio

# why doesn't python have setbuf??
oldprint = print
def print(*args):
	oldprint(*args, flush=True)

def read_text_file(path: str) -> str:
	with open(path) as f:
		return f.read()

async def markdown_to_html(path: str) -> str:
	proc = await asyncio.create_subprocess_exec('pandoc', '-o', '-', '--to=html', path,
		stdout=asyncio.subprocess.PIPE)
	stdout, _ = await proc.communicate()
	if proc.returncode:
		print(f'pandoc {path} exited with return code {proc.returncode}')
	return stdout.decode()
shutil.rmtree('dist', ignore_errors=True)
os.makedirs('dist', exist_ok=True)

version = subprocess.check_output(['../version.sh']).decode()
assert re.match(r'\d+\.\d+\.\d+', version), f'Version should be a version number. Got {version}.'

async def process_markdown():
	global readme, guide
	readme = markdown_to_html('../README.md')
	guide = markdown_to_html('../GUIDE.md')
	
	# pandoc is slow, so we run the pandoc conversions in parallel
	readme = await readme
	guide = await guide

print('Processing markdown files')
asyncio.run(process_markdown())

# The CSS is small enough that it's probably better just to include it inline
style = '<style>' + read_text_file('main.css') + '</style>'

readme_index = []
outputting = False
for line in readme.split('\n'):
	if line == '<!-- WEBSITE INDEX.HTML ON -->':
		outputting = True
	if line == '<!-- WEBSITE INDEX.HTML OFF -->':
		outputting = False
	if outputting:
		readme_index.append(line)
readme_index = '\n'.join(readme_index)

nav_template = read_text_file('template-nav.html')

def process_html_file(source: str) -> str:
	return source.replace('${VERSION}', version) \
		.replace('${NAV}', nav_template) \
		.replace('${README}', readme_index) \
		.replace('${STYLE}', style) \
		.replace('${GUIDE}', guide)

files = subprocess.check_output(['git', 'ls-files', '-z']).decode()


for f in files.split('\0'):
	if not f or f.startswith('template-') or f in {'.', '..', 'dist', 'Makefile', 'make.py', 'main.css'}: continue
	if f.endswith('.html'):
		print('Processing HTML', f)
		source = read_text_file(f)
		with open('dist/' + f, 'w') as o:
			o.write(process_html_file(source))
	else:
		print('Copying file', f)
		shutil.copyfile(f, 'dist/' + f)

