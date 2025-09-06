#!/usr/bin/env python3

import subprocess
import os
import signal
import sys
import time

FSTEST_PATH = "./target/fstest"


def tabulate(table):
    # [..., [name, counts, descs[]], ...]

    if len(table[2]) == 0:
        table[2] = ['']
    status = False
    for desc in table[-1]:
        if not status:
            print(table[0].ljust(15) + table[1].ljust(15) +
                  table[2].ljust(15) + desc)
            status = True
        else:
            print(''.ljust(15) + ''.ljust(15) + ''.ljust(15) + desc)


def run_tests(directory="."):
    _path = "./pjdfstest/tests/" + directory

    print("Test Name".ljust(15) + "Pass/Total".ljust(15) +
          "Skipped".ljust(15) + "Failed Tests")

    for filename in sorted(os.listdir(_path)):
        if filename.endswith(".t"):
            fstest_proc = subprocess.Popen([FSTEST_PATH],
                                           stdin=subprocess.PIPE,
                                           stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE)

            time.sleep(1)

            faildescs = []

            try:
                ret = subprocess.run(["bash", os.path.join(_path, filename)],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, timeout=5)

            except subprocess.TimeoutExpired as e:
                ret = subprocess.CompletedProcess(
                    args=e.cmd,
                    stdout=e.stdout,
                    stderr=e.stderr,
                    returncode=None,
                )
                faildescs.append("Timed out.")

            passed = str(ret.stdout).count("ok")
            failed = str(ret.stderr).count("not ok")
            skipped = str(ret.stderr).count("EOPNOTSUPP")
            failed -= skipped

            if ret.stderr:
                error = ret.stderr.decode()

                for i in error.split("not ok"):
                    if "not ok" not in i and len(i) > 0 \
                            and "EOPNOTSUPP" not in i:
                        faildescs.append(i.strip().replace('\n', ''))

            tabulate([directory + '/' + filename,
                     f"{passed}/{passed + failed}", f"{skipped}", faildescs])

            shutdown_fstest(fstest_proc)


def shutdown_fstest(proc):
    try:
        proc.send_signal(signal.SIGINT)
    except Exception as e:
        print(f"Failed to send shutdown command: {e}")


def main():
    tests = sys.argv

    if len(sys.argv) < 2:
        print("No test specified")
        exit(1)

    print("Starting tests...")
    for test in tests[1:]:
        run_tests(test)

    time.sleep(1)


if __name__ == "__main__":
    main()
