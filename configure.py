# vim: set ts=2 sw=2 tw=99 noet:
import sys
try:
	from ambuild2 import run
except:
	try:
		import ambuild
		sys.stderr.write('It looks like you have AMBuild 1 installed, but this project uses AMBuild 2.\n')
		sys.stderr.write('Upgrade to the latest version of AMBuild to continue.\n')
	except:
		sys.stderr.write('AMBuild must be installed to build this project.\n')
		sys.stderr.write('http://www.alliedmods.net/ambuild\n')
	sys.exit(1)

run = run.PrepareBuild(sourcePath=sys.path[0])
run.default_build_folder = 'obj-' + run.target_platform
run.options.add_option('--sm-path', type=str, dest='sm_path', default=None, help='Path to SourceMod')
run.options.add_option('--enable-debug', action='store_const', const='1', dest='debug', help='Enable debugging symbols')
run.options.add_option('--enable-optimize', action='store_const', const='1', dest='opt', help='Enable optimization')

run.Configure()