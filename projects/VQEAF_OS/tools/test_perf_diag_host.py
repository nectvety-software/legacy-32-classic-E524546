#!/usr/bin/env python3
"""Opt-in diagnostic build using the SAME full firmware host test harness."""
from test_v14_build import FLAGS, main
if __name__=="__main__":
    FLAGS.append("-DVQEAF_PERF_DIAG=1")
    main()
