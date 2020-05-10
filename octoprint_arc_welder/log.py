# coding=utf-8
# #################################################################################
# Arc Welder: Anti-Stutter
#
# A plugin for OctoPrint that converts G0/G1 commands into G2/G3 commands where possible and ensures that the tool
# paths don't deviate by more than a predefined resolution.  This compresses the gcode file sice, and reduces reduces
# the number of gcodes per second sent to a 3D printer that supports arc commands (G2 G3)
#
# Copyright (C) 2020  Brad Hochgesang
# #################################################################################
# This program is free software:
# you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see the following:
# https://github.com/FormerLurker/ArcWelderPlugin/blob/master/LICENSE
#
# You can contact the author either through the git-hub repository, or at the
# following email address: FormerLurker@pm.me
##################################################################################
from __future__ import unicode_literals
import logging
import datetime as datetime
import os
import six
from octoprint.logging.handlers import (
    AsyncLogHandlerMixin,
    CleaningTimedRotatingFileHandler,
)

class Singleton(type):
    _instances = {}

    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            cls._instances[cls] = super(Singleton, cls).__call__(*args, **kwargs)
        return cls._instances[cls]
# custom log level - VERBOSE
VERBOSE = 5
DEBUG = logging.DEBUG
INFO = logging.INFO
WARNING = logging.WARNING
ERROR = logging.ERROR
CRITICAL = logging.CRITICAL
logging.addLevelName(VERBOSE, "VERBOSE")


def verbose(self, msg, *args, **kwargs):
    if self.isEnabledFor(VERBOSE):
        self.log(VERBOSE, msg, *args, **kwargs)


logging.Logger.verbose = verbose
# end custom log level - VERBOSE


def format_log_time(time_seconds):
    log_time = datetime.datetime.fromtimestamp(time_seconds)
    t = datetime.datetime.strftime(
        log_time, "%Y-%m-%d %H:%M:%S,{0:03}".format(int(log_time.microsecond / 1000))
    )
    return t


class ArcWelderFormatter(logging.Formatter):
    def __init__(
        self, fmt="%(asctime)s - %(name)s - %(levelname)s - %(message)s", datefmt=None
    ):
        super(ArcWelderFormatter, self).__init__(fmt=fmt, datefmt=datefmt)

    def formatTime(self, record, datefmt=None):
        ct = self.converter(record.created)
        if datefmt:
            s = ct.strftime(datefmt)
        else:
            return format_log_time(record.created)
        return s


class ArcWelderConsoleHandler(logging.StreamHandler, AsyncLogHandlerMixin):
    def __init__(self, *args, **kwargs):
        super(ArcWelderConsoleHandler, self).__init__(*args, **kwargs)


class ArcWelderFileHandler(CleaningTimedRotatingFileHandler, AsyncLogHandlerMixin):
    def __init__(self, *args, **kwargs):
        super(ArcWelderFileHandler, self).__init__(*args, **kwargs)

    def delete_all_backups(self):
        # clean up old files on handler start
        backup_count = self.backupCount
        self.backupCount = 0
        for s in self.getFilesToDelete():
            os.remove(s)
        self.backupCount = backup_count


@six.add_metaclass(Singleton)
class LoggingConfigurator(object):
    BACKUP_COUNT = 3

    def __init__(self, root_logger_name, log_entry_prefix, log_file_prefix):
        self.logging_formatter = ArcWelderFormatter()
        self._root_logger_name = root_logger_name  # "arc_welder"
        self._log_entry_prefix = log_entry_prefix  # "arc_welder."
        self._log_file_prefix = log_file_prefix  # "octoprint_arc_welder."
        self._root_logger = self._get_root_logger(self._root_logger_name)

        self._level = logging.DEBUG
        self._file_handler = None
        self._console_handler = None
        self.child_loggers = set()

    def _get_root_logger(self, name):
        """Get a logger instance that uses new-style string formatting"""
        log = logging.getLogger(name)
        log.setLevel(logging.NOTSET)
        log.propagate = False
        return log

    def get_logger_names(self):
        logger_names = []
        for logger_name in self.child_loggers:
            logger_names.append(logger_name)
        return logger_names

    def get_root_logger(self):
        return self._root_logger

    def get_logger(self, name):
        if name == self._root_logger_name:
            return self._root_logger

        if name.startswith(self._log_file_prefix):
            name = name[len(self._log_file_prefix):]

        full_name = "arc_welder." + name

        self.child_loggers.add(full_name)
        child = self._root_logger.getChild(name)
        return child

    def _remove_handlers(self):
        if self._file_handler is not None:
            self._root_logger.removeHandler(self._file_handler)
            self._file_handler = None

        if self._console_handler is not None:
            self._root_logger.removeHandler(self._console_handler)
            self._console_handler = None

    def _add_file_handler(self, log_file_path, log_level):
        self._file_handler = ArcWelderFileHandler(
            log_file_path, when="D", backupCount=LoggingConfigurator.BACKUP_COUNT
        )
        self._file_handler.setFormatter(self.logging_formatter)
        self._file_handler.setLevel(log_level)
        self._root_logger.addHandler(self._file_handler)

    def _add_console_handler(self, log_level):
        self._console_handler = ArcWelderConsoleHandler()
        self._console_handler.setFormatter(self.logging_formatter)
        self._console_handler.setLevel(log_level)
        self._root_logger.addHandler(self._console_handler)

    def do_rollover(self, clear_all=False):
        if self._file_handler is None:
            return
        # To clear everything, we'll roll over every file
        self._file_handler.doRollover()
        if clear_all:
            self._file_handler.delete_all_backups()

    def configure_loggers(self, log_file_path=None, logging_settings=None):
        default_log_level = logging.DEBUG
        log_to_console = True
        if logging_settings is not None:
            if "default_log_level" in logging_settings:
                default_log_level = logging_settings["default_log_level"]
            if "log_to_console" in logging_settings:
                log_to_console = logging_settings["log_to_console"]

        # clear any handlers from the root logger
        self._remove_handlers()
        # set the log level
        self._root_logger.setLevel(logging.NOTSET)

        if log_file_path is not None:
            # ensure that the logging path and file exist
            directory = os.path.dirname(log_file_path)
            import distutils.dir_util

            distutils.dir_util.mkpath(directory)
            if not os.path.isfile(log_file_path):
                open(log_file_path, "w").close()

            # add the file handler
            self._add_file_handler(log_file_path, logging.NOTSET)

        # if we are logging to console, add the console logging handler
        if log_to_console:
            self._add_console_handler(logging.NOTSET)
        for logger_full_name in self.child_loggers:

            if logger_full_name.startswith(self._log_entry_prefix):
                logger_name = logger_full_name[len(self._log_entry_prefix):]
            else:
                logger_name = logger_full_name
            if logging_settings is not None:
                current_logger = self._root_logger.getChild(logger_name)
                found_enabled_logger = None
                if "enabled_loggers" in logging_settings:
                    for enabled_logger in logging_settings["enabled_loggers"]:
                        if enabled_logger["name"] == logger_full_name:
                            found_enabled_logger = enabled_logger
                            break

                if found_enabled_logger is None or current_logger.level > logging.ERROR:
                    current_logger.setLevel(logging.ERROR)

                elif found_enabled_logger is not None:
                    current_logger.setLevel(found_enabled_logger["log_level"])
                else:
                    # log level critical + 1 will not log anything
                    current_logger.setLevel(logging.CRITICAL + 1)
            else:
                current_logger = self._root_logger.getChild(logger_name)
                current_logger.setLevel(default_log_level)
