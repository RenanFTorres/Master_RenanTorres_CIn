#!/usr/bin/env python

from __future__ import division, print_function, unicode_literals

import datetime
import hashlib
import os
import re

import ttfw_idf
from tiny_test_fw import Utility


def verify_elf_sha256_embedding(dut):
    elf_file = os.path.join(dut.app.binary_path, 'blink.elf')
    sha256 = hashlib.sha256()
    with open(elf_file, 'rb') as f:
        sha256.update(f.read())
    sha256_expected = sha256.hexdigest()

    dut.reset()
    sha256_reported = dut.expect(re.compile(r'ELF file SHA256:\s+([a-f0-9]+)'), timeout=5)[0]

    Utility.console_log('ELF file SHA256: %s' % sha256_expected)
    Utility.console_log('ELF file SHA256 (reported by the app): %s' % sha256_reported)
    # the app reports only the first several hex characters of the SHA256, check that they match
    if not sha256_expected.startswith(sha256_reported):
        raise ValueError('ELF file SHA256 mismatch')


@ttfw_idf.idf_example_test(env_tag='Example_GENERIC', target=['esp32', 'esp32c3'])
def test_examples_blink(env, extra_data):
    dut = env.get_dut('blink', 'examples/get-started/blink')
    binary_file = os.path.join(dut.app.binary_path, 'blink.bin')
    bin_size = os.path.getsize(binary_file)
    ttfw_idf.log_performance('blink_bin_size', '{}KB'.format(bin_size // 1024))

    dut.start_app()

    verify_elf_sha256_embedding(dut)

@ttfw_idf.idf_example_test(env_tag='Example_WIFI_Protocols')
def test_examples_sntp(env, extra_data):

    dut = env.get_dut('sntp', 'examples/protocols/sntp')
    dut.start_app()

    dut.expect_all('Time is not set yet. Connecting to WiFi and getting time over NTP.',
                   'Initializing SNTP',
                   'Waiting for system time to be set... (1/10)',
                   'Notification of a time synchronization event',
                   timeout=60)

    TIME_FORMAT = '%a %b %d %H:%M:%S %Y'
    TIME_FORMAT_REGEX = r'\w+\s+\w+\s+\d{1,2}\s+\d{2}:\d{2}:\d{2} \d{4}'
    TIME_DIFF = datetime.timedelta(seconds=10 + 2)  # cpu spends 10 seconds in deep sleep
    NY_time = None
    SH_time = None

    def check_time(prev_NY_time, prev_SH_time):
        NY_str = dut.expect(re.compile(r'The current date/time in New York is: ({})'.format(TIME_FORMAT_REGEX)))[0]
        SH_str = dut.expect(re.compile(r'The current date/time in Shanghai is: ({})'.format(TIME_FORMAT_REGEX)))[0]
        Utility.console_log('New York: "{}"'.format(NY_str))
        Utility.console_log('Shanghai: "{}"'.format(SH_str))
        dut.expect('Entering deep sleep for 10 seconds')
        Utility.console_log('Sleeping...')
        new_NY_time = datetime.datetime.strptime(NY_str, TIME_FORMAT)
        new_SH_time = datetime.datetime.strptime(SH_str, TIME_FORMAT)

        # The initial time is not checked because datetime has problems with timezones
        assert prev_NY_time is None or new_NY_time - prev_NY_time < TIME_DIFF
        assert prev_SH_time is None or new_SH_time - prev_SH_time < TIME_DIFF

        return (new_NY_time, new_SH_time)

    NY_time, SH_time = check_time(NY_time, SH_time)
    for i in range(2, 4):
        dut.expect('example: Boot count: {}'.format(i), timeout=30)
        NY_time, SH_time = check_time(NY_time, SH_time)


if __name__ == '__main__':
    test_examples_blink()
    test_examples_sntp()
