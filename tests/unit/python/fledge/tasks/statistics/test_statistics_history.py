# -*- coding: utf-8 -*-

# FLEDGE_BEGIN
# See: http://fledge-iot.readthedocs.io/
# FLEDGE_END

"""Test tasks/statistics/statistics_history.py"""

import asyncio
from unittest.mock import patch, MagicMock
import pytest
import sys

import ast
from fledge.common.logger import FLCoreLogger
from fledge.common.process import FledgeProcess
from fledge.tasks.statistics.statistics_history import StatisticsHistory
from fledge.common.storage_client.storage_client import StorageClientAsync

__author__ = "Vaibhav Singhal"
__copyright__ = "Copyright (c) 2017 OSIsoft, LLC"
__license__ = "Apache 2.0"
__version__ = "${VERSION}"


pytestmark = pytest.mark.asyncio


async def mock_coro(*args, **kwargs):
    if len(args) > 0:
        return args[0]
    else:
        return ""


class TestStatisticsHistory:
    """Test the units of statistics_history.py
    """

    async def test_init(self):
        """Test that creating an instance of StatisticsHistory calls init of FledgeProcess and creates loggers"""
        with patch.object(FledgeProcess, "__init__") as mock_process:
            with patch.object(FLCoreLogger, "get_logger") as log:
                sh = StatisticsHistory()
                assert isinstance(sh, StatisticsHistory)
            log.assert_called_once_with("StatisticsHistory")
        mock_process.assert_called_once_with()

    async def test_update_previous_value(self):
        # Changed in version 3.8: patch() now returns an AsyncMock if the target is an async function.
        if sys.version_info.major == 3 and sys.version_info.minor >= 8:
            _rv = await mock_coro(None)
        else:
            _rv = asyncio.ensure_future(mock_coro(None))
        
        with patch.object(FledgeProcess, '__init__'):
            with patch.object(FLCoreLogger, "get_logger"):
                sh = StatisticsHistory()
                sh._storage_async = MagicMock(spec=StorageClientAsync)
                payload = {'updates': [{'where': {'value': 'Bla', 'condition': '=', 'column': 'key'}, 'values': {'previous_value': 1}}]}
                with patch.object(sh._storage_async, "update_tbl", return_value=_rv) as patch_storage:
                    await sh._bulk_update_previous_value(payload)
                args, kwargs = patch_storage.call_args
                assert "statistics" == args[0]
                payload = ast.literal_eval(args[1])
                assert "Bla" == payload["updates"][0]["where"]["value"]
                assert 1 == payload["updates"][0]["values"]["previous_value"]

    async def test_run(self):
        with patch.object(FledgeProcess, '__init__'):
            with patch.object(FLCoreLogger, "get_logger"):
                sh = StatisticsHistory()
                sh._storage_async = MagicMock(spec=StorageClientAsync)
                retval = {'count': 2,
                          'rows': [{'description': 'Readings removed from the buffer by the purge process',
                                    'value': 0, 'key': 'PURGED', 'previous_value': 0,
                                    'ts': '2018-08-31 17:03:17.597055+05:30'},
                                   {'description': 'Readings received by Fledge',
                                    'value': 0, 'key': 'READINGS', 'previous_value': 0,
                                    'ts': '2018-08-31 17:03:17.597055+05:30'
                                    }]
                          }
                # Changed in version 3.8: patch() now returns an AsyncMock if the target is an async function.
                if sys.version_info.major == 3 and sys.version_info.minor >= 8:
                    _rv1 = await mock_coro(retval)
                    _rv2 = await mock_coro(None)
                else:
                    _rv1 = asyncio.ensure_future(mock_coro(retval))
                    _rv2 = asyncio.ensure_future(mock_coro(None))

                with patch.object(sh._storage_async, "query_tbl", return_value=_rv1) as mock_keys:
                    with patch.object(sh, "_bulk_update_previous_value", return_value=_rv2) as mock_update:
                        with patch.object(sh._storage_async, "insert_into_tbl", return_value=_rv2) as mock_bulk_insert:
                            await sh.run()
                    assert 1 == mock_bulk_insert.call_count
                    assert 1 == mock_update.call_count
                mock_keys.assert_called_once_with('statistics')
