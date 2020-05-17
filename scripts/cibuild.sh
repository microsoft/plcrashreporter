#!/bin/bash

sh scripts/setup.sh
bundle exec pod lib lint --allow-warnings --no-clean --verbose
