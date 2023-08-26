# update windows installer ProductVersion/ProductCode/PackageCode
# this needs to be done for every release of ted

import uuid
import re
from datetime import datetime

timestamp = datetime.utcnow()
# will break in 2100. FUCK YOU PEOPEL OF THE FUTURE!!
version_start = '%02d.%02d.%02d' % (timestamp.year % 100, timestamp.month, timestamp.day)
product_code = str(uuid.uuid4()).upper()
package_code = str(uuid.uuid4()).upper()

PATH = 'windows_installer/ted/ted.vdproj'

f = open(PATH)
text = f.read()
f.close()

text = re.sub(r'"ProductCode" = "8:\{.*\}"', '"ProductCode" = "8:{%s}"' % product_code, text)
text = re.sub(r'"PackageCode" = "8:\{.*\}"', '"PackageCode" = "8:{%s}"' % package_code, text)
curr_version = re.search('"ProductVersion" = "8:([^"]*)"', text).group(1)
curr_version_start = curr_version[:len(version_start)]
daily_id = 0
if curr_version_start > version_start:
	print('time went backwards. aborting.')
	exit(1)
elif curr_version_start == version_start:
	daily_id = int(curr_version[len(version_start):]) + 1
	if daily_id >= 100:
			
		print('''you've updated the windows installer 100 times today.
i think you should take a break.
aborting.
''')
		exit(1)

version = '%s%02d' % (version_start, daily_id)
text = re.sub(r'"ProductVersion" = "8:([^"]*)"', r'"ProductVersion" = "8:%s"' % version, text)
f = open(PATH, 'w')
f.write(text)
f.close()
