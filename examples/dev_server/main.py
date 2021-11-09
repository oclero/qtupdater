from http.server import HTTPServer
from server import Server
import logging
import argparse
import os
import sys

DEFAULT_HOST_NAME = 'localhost'
DEFAULT_PORT_NUMBER = 8000
unresolved_root_dir = os.path.dirname(os.path.realpath(__file__)) + './../tmp/server/'
DEFAULT_ROOT_DIR = os.path.realpath(unresolved_root_dir)

if __name__ == '__main__':
	# Configure logging.
	log_filepath = os.path.dirname(os.path.realpath(__file__)) + '/server.log'
	if os.path.exists(log_filepath):
		os.remove(log_filepath)
	logging.basicConfig(
		level=logging.DEBUG,
		format="[%(asctime)s] [%(levelname)s] %(message)s",
		handlers=[
			logging.FileHandler(log_filepath),
			logging.StreamHandler()
		]
	)

	# Parse arguments.
	parser = argparse.ArgumentParser(description='Basic auto-update server for development purposes')
	parser.add_argument('--dir', type=str, help='Directory where the update files are.')
	parser.add_argument('--port', type=int, help='Port number.')
	parser.add_argument('--address', type=str, help='Address.')
	args = parser.parse_args()

	if args.dir is not None and len(args.dir) > 0 and not os.path.isdir(args.dir):
		logging.debug('Root directory does not exist')
		sys.exit()

	host_name = DEFAULT_HOST_NAME
	port_number = DEFAULT_PORT_NUMBER
	root_dir = DEFAULT_ROOT_DIR
	if args.port is not None:
		port_number = args.port
	if args.address is not None:
		host_name = args.address
	if args.dir is not None and len(args.dir) > 0:
		root_dir = os.path.realpath(args.dir)

	# Start server.
	Server.root_dir = root_dir
	httpd = HTTPServer((host_name, port_number), Server)
	logging.debug('Server started @ \'%s:%s\' \'%s\'' % (host_name, port_number, Server.root_dir))

	try:
		httpd.serve_forever()
	except KeyboardInterrupt:
		print('')

	httpd.server_close()
	logging.debug('Server stopped')
