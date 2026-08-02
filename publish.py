#!/usr/bin/env python3

import subprocess, re, sys, os, shutil

# Script to publish new version of ted to the various places.
def confirm(prompt):
	ok = input(prompt)
	if not ok.lower().startswith('y'):
		print('Aborting.')
		sys.exit(1)

version = subprocess.check_output('./version.sh').decode().strip()
assert not '\n' in version
assert re.match(r'^\d+\.\d+\.\d+$', version), 'Unexpected version ' + version

confirm('Publish ted v. ' + version + '? ')

with open('CHANGELOG.md') as f:
	assert f.read().startswith('## ' + version + ' '), \
		'Changelog should start with ## ' + version

print('Copying over remote files to local website/releases/…')
subprocess.run(['rclone', 'copy', '-P', 'linode-fr:/ted.pommicket.com/releases/', 'website/releases/'])

print('Building .deb files…')
subprocess.run(['make', 'ted-versioned.deb'])
subprocess.run(['make', 'ted-static-sdl-versioned.deb'])

print('Copying over ted.msi…')
subprocess.run(['scp', 'git:ted.msi', '.'])
checksum = subprocess.check_output(['sha256sum', 'ted.msi']).decode().strip().split()[0]
confirm('Checksum is\n' + checksum + '\nIs this correct? ')

print('Moving around files…')
os.rename('ted.msi', f'website/releases/ted_{version}_amd64.msi')
for package in ['ted', 'ted-static-sdl']:
	for dest in ['website/releases', '/p/repo/pool/main']:
		shutil.copyfile(f'{package}_{version}-1_amd64.deb', f'{dest}/{package}_{version}-1_amd64.deb')

print('Building website…')
subprocess.run(['cargo', 'run'], cwd='website')

tags = subprocess.check_output(['git', 'tag']).decode().split('\n')
if version not in tags:
	print('Creating tag…')
	subprocess.run(['git', 'tag', '-s', version])

confirm(f'\x1b[1mThis is the point of no return.\x1b[0m Push trunk and {version} everywhere? ')
for remote in ['server', 'origin', 'github']:
	subprocess.run(['git', 'push', remote, 'trunk', version])

confirm('Publish website? ')
subprocess.run(['./website/publish.sh'])

confirm('Publish to repo? ')
subprocess.run(['../repo/release.sh'], cwd='../repo')

