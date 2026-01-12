#!/usr/bin/env python3
"""Development server for testing Qt auto-update functionality."""

from http.server import HTTPServer
from pathlib import Path
import logging
import argparse
import sys

from server import Server

DEFAULT_HOST = 'localhost'
DEFAULT_PORT = 8000
DEFAULT_ROOT_DIR = Path(__file__).parent / 'public'


def setup_logging() -> None:
  """Configure logging to both file and console."""
  log_file = Path(__file__).parent / 'server.log'
  log_file.unlink(missing_ok=True)

  logging.basicConfig(
      level=logging.DEBUG,
      format="[%(asctime)s] [%(levelname)s] %(message)s",
      handlers=[
          logging.FileHandler(log_file),
          logging.StreamHandler()
      ]
  )


def parse_arguments() -> argparse.Namespace:
  """Parse command-line arguments."""
  parser = argparse.ArgumentParser(
      description='Basic auto-update server for development purposes'
  )
  parser.add_argument(
      '--dir',
      type=Path,
      default=DEFAULT_ROOT_DIR,
      help='Directory where the update files are located'
  )
  parser.add_argument(
      '--port',
      type=int,
      default=DEFAULT_PORT,
      help='Port number'
  )
  parser.add_argument(
      '--address',
      type=str,
      default=DEFAULT_HOST,
      help='Server address'
  )
  return parser.parse_args()


def main() -> None:
  """Main entry point for the development server."""
  setup_logging()
  args = parse_arguments()

  # Validate root directory.
  root_dir = args.dir.resolve()
  if not root_dir.is_dir():
    logging.error(f'Root directory does not exist: {root_dir}')
    sys.exit(1)

  # Start server.
  Server.root_dir = str(root_dir)
  server_address = (args.address, args.port)
  httpd = HTTPServer(server_address, Server)
  logging.info(
    f'Server started @ http://{args.address}:{args.port} serving {root_dir}')

  try:
    httpd.serve_forever()
  except KeyboardInterrupt:
    print('\nShutting down...')
  finally:
    httpd.server_close()
    logging.info('Server stopped')


if __name__ == '__main__':
  main()
