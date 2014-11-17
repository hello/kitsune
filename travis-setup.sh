#!/bin/bash

wget https://www.dropbox.com/s/2zr5iiuk8tezkh8/travisti.tar.gz
tar -xzf travisti.tar.gz -C ~/

echo "branch $TRAVIS_BRANCH"
echo "tag $TRAVIS_TAG"
echo "commit $TRAVIS_COMMIT"
