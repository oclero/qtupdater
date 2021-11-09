import os
from urllib import parse
from distutils import version
import glob
import json
from typing import Dict, List
from pathlib import Path
from http.server import BaseHTTPRequestHandler
import logging

###################################################################################################
# Constants.

APP_NAME = 'your-app'
PLATFORM_WINDOWS = 'win'
PLATFORM_MACOS = 'mac'
ALIAS_VERSION_LATEST = 'latest'
ALIAS_BRANCH_MAIN = 'release'

###################################################################################################
# Classes.

class VersionInformation(object):
	version = version.StrictVersion('0.0.0')
	json_filepath = ''
	installer_filepath = ''
	changelog_filepath = ''
	json_data = None

	def __init__(self, version, json_filepath = '', installer_filepath = '', changelog_filepath = '', json_data = None):
		self.version = version
		self.json_filepath = json_filepath
		self.installer_filepath = installer_filepath
		self.changelog_filepath = changelog_filepath
		self.json_data = json_data

	def __repr__(self):
		return str(self.__dict__)

class RequestResult(object):
	success = False
	message = '' # Error message.
	filepath = '' # Path on the server.
	content = bytes(0) # Request data.
	size = 0 # Data size.
	content_type = '' # MIME type.

	def __init__(self, success, message, filepath = '', content = bytes(0), size = 0, content_type = ''):
		self.success = success
		self.message = message
		self.filepath = filepath
		self.content = content
		self.size = size
		self.content_type = content_type

	def __repr__(self):
		return str(self.__dict__)

###################################################################################################
# Internal functions.

def get_available_versions(root_dir, app_name, platform, server_address) -> List[VersionInformation]:
	available_versions = []

	# Find all available versions.
	versions_dir = root_dir + '/' + app_name + '/' + platform
	json_files = glob.glob(os.path.abspath(versions_dir + '/*.json'))
	for json_file in json_files:
		try:
			with open(json_file, 'rb') as f:
				data = json.load(f)
				data_version = version.StrictVersion(data['version'])

				# Add field 'url' that contains a public link to the installer file.
				file_name = Path(json_file).stem
				file_extension = '.exe' if platform == 'win' else '.dmg'
				installer_file = 'http://' + server_address + '/' + app_name + '/' + platform + '/' + file_name + file_extension
				data['installerUrl'] = installer_file

				# Add fild 'changelog' that contains a public link to the changelog file.
				changelog_file = 'http://' + server_address + '/' + app_name + '/' + platform + '/' + file_name + '.md'
				data['changelogUrl'] = changelog_file

				available_versions.append(VersionInformation(data_version, json_file, installer_file, changelog_file, data))
		except:
			# Silently ignore IO errors.
			pass

	# Sort them from older to newer.
	available_versions.sort(key=lambda item: item.version)

	return available_versions

def get_latest_version(available_versions) -> version.StrictVersion:
	if len(available_versions) > 0:
		return available_versions[-1]
	else:
		return None

def get_params(query) -> Dict[str, str]:
	# Parse query.
	query_params = parse.parse_qs(query)

	# Flatten lists by keeping only the first element.
	for key in query_params.keys():
		if isinstance(query_params[key], list):
			query_params[key] = query_params[key][0]

	return query_params

def get_extension(path) -> str:
	return os.path.splitext(path)[1]

def get_version(query_params, latest_version) -> version.StrictVersion:
	if latest_version is None:
		return None

	version_str = query_params.get('version', ALIAS_VERSION_LATEST)

	# If it is alias keyword, get actual latest version.
	if version_str == ALIAS_VERSION_LATEST:
		return latest_version

	# Check if version is valid.
	query_version = None
	try:
		query_version = version.StrictVersion(version_str)
	except:
		query_version = None

	return query_version

def handle_appcast_request(request_url, root_dir, server_address) -> RequestResult:
	# Check path validity.
	request_path_elements = request_url.path.split('/')
	if len(request_path_elements) != 3:
		return RequestResult(False, 'Invalid URL')

	request_app_name = request_path_elements[1]
	if request_app_name != APP_NAME:
		return RequestResult(False, 'Invalid app name: %s' % (request_app_name))

	request_platform = request_path_elements[2]
	if request_platform != PLATFORM_WINDOWS and request_platform != PLATFORM_MACOS:
		return RequestResult(False, 'Invalid platform: %s' % (request_platform))

	# Check query parameters.
	request_query_params = get_params(request_url.query)

	# Check if version is valid.
	available_versions = get_available_versions(root_dir, request_app_name, request_platform, server_address)
	latest_version = get_latest_version(available_versions)

	if latest_version is None:
		return RequestResult(False, 'No latest version available')

	request_version = get_version(request_query_params, latest_version.version)
	if request_version is None:
		return RequestResult(False, 'Invalid version: %s' % (request_query_params['version']))

	if request_version > latest_version.version:
		return RequestResult(False, 'Version not available (too high): %s' % (request_version))

	# Check if version is available.
	matching_versions = [x for x in available_versions if x.version == request_version]
	if len(matching_versions) == 0:
		return RequestResult(False, 'Version not available')

	# Get JSON content.
	matching_version = matching_versions[0]
	result_filepath = matching_version.json_filepath
	result_content = json.dumps(matching_version.json_data).encode('utf-8')
	result_size = len(result_content)
	result_content_type = 'application/json'

	return RequestResult(True, '', result_filepath, result_content, result_size, result_content_type)

def handle_file_request(request_url, root_dir) -> RequestResult:
	# Check path validity.
	request_path_elements = request_url.path.split('/')
	if len(request_path_elements) != 4:
		return RequestResult(False, 'Invalid URL')

	request_app_name = request_path_elements[1]
	if request_app_name != APP_NAME:
		return RequestResult(False, 'Invalid app name: %s' % (request_app_name))

	request_platform = request_path_elements[2]
	if request_platform != PLATFORM_WINDOWS and request_platform != PLATFORM_MACOS:
		return RequestResult(False, 'Invalid platform: %s' % (request_platform))

	request_file = request_path_elements[3]
	result_filepath = root_dir + '/' + request_app_name + '/' + request_platform + '/' + request_file

	if not os.path.isfile(result_filepath):
		return RequestResult(False, 'File does not exist')

	# Get file content.
	result_content = bytes(0)
	result_size = 0
	result_content_type = ''

	try:
		result_size = os.path.getsize(result_filepath)
		with open(result_filepath, 'rb') as f:
			result_content = f.read()
			result_size = len(result_content)
	except:
		return RequestResult(False, 'Cannot read file content')

	# Get file content-type.
	mimetypes = {
		'.exe': 'application/vnd.microsoft.portable-executable',
		'.dmg': 'application/vnd.apple.diskimage',
		'.md': 	'text/markdown',
	}
	file_extension = get_extension(request_file)
	result_content_type = mimetypes[file_extension]

	return RequestResult(True, '', result_filepath, result_content, result_size, result_content_type)

###################################################################################################

class Server(BaseHTTPRequestHandler):
	root_dir = '.'

	def handle_request(self, url, root_dir, server_address) -> RequestResult:
		request_url = parse.urlsplit(url)
		request_extension = get_extension(request_url.path)

		# Check if the URL is valid.
		if '..' in request_url.path or '/.' in request_url.path or './' in request_url.path:
			return RequestResult(False, 'Invalid URL')

		# Handle request.
		if request_extension == '':
			# Case: request for JSON file.
			return handle_appcast_request(request_url, root_dir, server_address)
		elif request_extension == '.exe' or request_extension == '.dmg' or request_extension == '.md':
			# Case: request to download file.
			return handle_file_request(request_url, root_dir)
		else:
			return RequestResult(False, 'Invalid URL')

	def do_GET(self):
		logging.debug('Request received: \'%s\'' % (self.path))
		request_result = self.handle_request(self.path, self.root_dir, "%s:%s" % self.server.server_address)

		if request_result.success:
			logging.debug('Request succeeded (\'%s\')' % (os.path.basename(request_result.filepath)))
		else:
			logging.debug('Request failed (%s)' % (request_result.message))

		if request_result.success:
			self.send_response(200)
			self.send_header('Accept', request_result.content_type)
			self.send_header('Content-Type', request_result.content_type)
			self.send_header('Content-Length', str(request_result.size))
			self.end_headers()
			self.wfile.write(request_result.content)

		else:
			self.send_response(404)
			self.send_header('Content-type', 'text/html')
			self.end_headers()
