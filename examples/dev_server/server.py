"""HTTP server for serving Qt updater test files."""

from dataclasses import dataclass
from pathlib import Path
from urllib import parse
from packaging import version
import json
import logging
from http.server import BaseHTTPRequestHandler

# Constants.
ALIAS_VERSION_LATEST = 'latest'
PACKAGE_EXTENSIONS = {'.exe', '.dmg'}
CHANGELOG_EXTENSION = '.md'
MIMETYPES = {
    '.exe': 'application/vnd.microsoft.portable-executable',
    '.dmg': 'application/vnd.apple.diskimage',
    '.md': 'text/markdown',
    '.json': 'application/json',
}


@dataclass
class VersionInformation:
  """Information about a specific version."""
  version: version.Version
  json_filepath: Path
  installer_url: str
  changelog_url: str
  json_data: dict


@dataclass
class RequestResult:
  """Result of processing an HTTP request."""
  success: bool
  message: str = ''
  filepath: Path | None = None
  content: bytes = b''
  content_type: str = ''

  @property
  def size(self) -> int:
    return len(self.content)


def get_available_versions(root_dir: str, server_address: str) -> list[VersionInformation]:
  """Find all available versions in the root directory."""
  available_versions = []
  root_path = Path(root_dir)

  for json_file in root_path.glob('*.json'):
    try:
      with open(json_file, 'r', encoding='utf-8') as f:
        data = json.load(f)
        data_version = version.parse(data['version'])
        file_stem = json_file.stem

        # Check for installer file.
        installer_file = None
        for ext in PACKAGE_EXTENSIONS:
          potential_installer = root_path / f'{file_stem}{ext}'
          if potential_installer.is_file():
            installer_file = potential_installer
            break

        if not installer_file:
          continue

        # Check for changelog file.
        changelog_file = root_path / f'{file_stem}{CHANGELOG_EXTENSION}'
        if not changelog_file.is_file():
          continue

        # Build URLs.
        installer_url = f'http://{server_address}/{installer_file.name}'
        changelog_url = f'http://{server_address}/{changelog_file.name}'

        data['installerUrl'] = installer_url
        data['changelogUrl'] = changelog_url

        available_versions.append(VersionInformation(
            version=data_version,
            json_filepath=json_file,
            installer_url=installer_url,
            changelog_url=changelog_url,
            json_data=data
        ))
    except (json.JSONDecodeError, KeyError, ValueError) as e:
      logging.warning(f'Skipping invalid version file {json_file}: {e}')

  # Sort from oldest to newest.
  available_versions.sort(key=lambda item: item.version)
  return available_versions


def get_latest_version(available_versions: list[VersionInformation]) -> VersionInformation | None:
  """Get the latest version from the list."""
  return available_versions[-1] if available_versions else None


def get_query_params(query: str) -> dict[str, str]:
  """Parse query string into a dictionary."""
  query_params = parse.parse_qs(query)
  # Flatten lists by keeping only the first element.
  return {key: values[0] for key, values in query_params.items()}


def get_requested_version(
    query_params: dict[str, str],
    latest_version: version.Version
) -> version.Version | None:
  """Parse and validate the requested version."""
  version_str = query_params.get('version', ALIAS_VERSION_LATEST)

  if version_str == ALIAS_VERSION_LATEST:
    return latest_version

  try:
    return version.parse(version_str)
  except version.InvalidVersion:
    return None


def handle_appcast_request(
    request_url: parse.SplitResult,
    root_dir: str,
    server_address: str
) -> RequestResult:
  """Handle request for version JSON (appcast)."""
  # Validate URL path.
  path_elements = [p for p in request_url.path.split('/') if p]
  if len(path_elements) != 0:  # Should be just '/'
    return RequestResult(False, 'Invalid URL for appcast')

  # Get available versions.
  available_versions = get_available_versions(root_dir, server_address)
  latest = get_latest_version(available_versions)

  if not latest:
    return RequestResult(False, 'No versions available')

  # Parse query parameters.
  query_params = get_query_params(request_url.query)
  requested_version = get_requested_version(query_params, latest.version)

  if not requested_version:
    return RequestResult(False, f'Invalid version: {query_params.get("version")}')

  if requested_version > latest.version:
    return RequestResult(False, f'Version not available: {requested_version}')

  # Find matching version.
  matching = [v for v in available_versions if v.version == requested_version]
  if not matching:
    return RequestResult(False, f'Version not available: {requested_version}')

  # Return JSON content.
  matching_version = matching[0]
  content = json.dumps(matching_version.json_data, indent=2).encode('utf-8')

  return RequestResult(
      success=True,
      filepath=matching_version.json_filepath,
      content=content,
      content_type=MIMETYPES['.json']
  )


def handle_file_request(request_url: parse.SplitResult, root_dir: str) -> RequestResult:
  """Handle request for a file download."""
  # Validate URL path
  path_elements = [p for p in request_url.path.split('/') if p]
  if len(path_elements) != 1:
    return RequestResult(False, 'Invalid file URL')

  # Security check: prevent directory traversal
  filename = path_elements[0]
  if '..' in filename or filename.startswith('.'):
    return RequestResult(False, 'Invalid filename')

  # Check file existence
  file_path = Path(root_dir) / filename
  if not file_path.is_file() or not file_path.resolve().is_relative_to(Path(root_dir).resolve()):
    return RequestResult(False, 'File does not exist')

  # Read file content
  try:
    content = file_path.read_bytes()
  except OSError as e:
    return RequestResult(False, f'Cannot read file: {e}')

  # Determine content type
  content_type = MIMETYPES.get(file_path.suffix, 'application/octet-stream')

  return RequestResult(
      success=True,
      filepath=file_path,
      content=content,
      content_type=content_type
  )


class Server(BaseHTTPRequestHandler):
  """HTTP request handler for the development server."""
  root_dir: str = '.'

  def handle_request(self, url: str, root_dir: str, server_address: str) -> RequestResult:
    """Route request to appropriate handler."""
    request_url = parse.urlsplit(url)
    extension = Path(request_url.path).suffix

    # Security check.
    if '..' in request_url.path or '/.' in request_url.path:
      return RequestResult(False, 'Invalid URL (security)')

    # Route based on extension.
    if extension == '':
      return handle_appcast_request(request_url, root_dir, server_address)
    elif extension in PACKAGE_EXTENSIONS or extension == CHANGELOG_EXTENSION:
      return handle_file_request(request_url, root_dir)
    else:
      return RequestResult(False, f'Unsupported file type: {extension}')

  def do_GET(self) -> None:
    """Handle GET requests."""
    logging.info(f'Request: {self.path}')
    server_address = f'{self.server.server_address[0]}:{self.server.server_address[1]}'
    result = self.handle_request(self.path, self.root_dir, server_address)

    if result.success:
      logging.info(
        f'Success: {result.filepath.name if result.filepath else "appcast"}')
      self.send_response(200)
      self.send_header('Content-Type', result.content_type)
      self.send_header('Content-Length', str(result.size))
      self.send_header('Access-Control-Allow-Origin', '*')
      self.end_headers()
      self.wfile.write(result.content)
    else:
      logging.warning(f'Failed: {result.message}')
      self.send_response(404)
      self.send_header('Content-Type', 'text/plain')
      self.end_headers()
      self.wfile.write(result.message.encode('utf-8'))

  def log_message(self, format: str, *args) -> None:
    """Override to prevent duplicate logging."""
    pass
