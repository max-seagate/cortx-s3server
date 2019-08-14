"""This class provides Key-value REST API i.e. GET,PUT and DELETE."""
import logging

from eos_core_client import EOSCoreClient
from eos_get_kv_response import EOSCoreGetKVResponse
from eos_core_error_respose import EOSCoreErrorResponse
from eos_core_success_response import EOSCoreSuccessResponse

# EOSCoreKVApi supports key-value REST-API's Put, Get & Delete


class EOSCoreKVApi(EOSCoreClient):
    """EOSCoreKVApi provides key-value REST-API's Put, Get & Delete."""
    _logger = None

    def __init__(self, config, logger=None):
        """Initialise logger and config."""
        if (logger is None):
            self._logger = logging.getLogger("EOSCoreKVApi")
        else:
            self._logger = logger
        self._logger = logging.getLogger()
        self.config = config
        super(EOSCoreKVApi, self).__init__(self.config, logger=self._logger)

    def put(self, index=None, key=None, value=None):
        """Perform PUT request and generate response."""
        if index is None:
            self._logger.error("Index Id is required.")
            return None
        if key is None:
            self._logger.error("Key is required")
            return None

        request_body = value
        request_uri = '/indexes/' + index + '/' + key
        try:
            response = super(EOSCoreKVApi, self).put(request_uri, request_body)
        except Exception as ex:
            self._logger.error(str(ex))
            return None

        if response['status'] == 201:
            self._logger.info("Key value details added successfully.")
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info('Failed to add key value details.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])

    def get(self, index=None, key=None):
        """Perform GET request and generate response."""
        if index is None:
            self._logger.error("Index Id is required.")
            return None
        if key is None:
            self._logger.error("Key is required")
            return None

        request_uri = '/indexes/' + index + '/' + key

        try:
            response = super(EOSCoreKVApi, self).get(request_uri)
        except Exception as ex:
            self._logger.error(str(ex))
            return None

        if response['status'] == 200:
            self._logger.info("Get kv operation successfully.")
            return True, EOSCoreGetKVResponse(key, response['body'])
        else:
            self._logger.info('Failed to get kv details.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])

    def delete(self, index=None, key=None):
        """Perform DELETE request and generate response."""
        if index is None:
            self._logger.error("Index Id is required.")
            return None
        if key is None:
            self._logger.error("Key is required")
            return None

        request_uri = '/indexes/' + index + '/' + key
        try:
            response = super(EOSCoreKVApi, self).delete(request_uri)
        except Exception as ex:
            self._logger.error(str(ex))
            return None

        if response['status'] == 204:
            self._logger.info('Key value deleted.')
            return True, EOSCoreSuccessResponse(response['body'])
        else:
            self._logger.info('Failed to delete key value.')
            return False, EOSCoreErrorResponse(
                response['status'], response['reason'], response['body'])