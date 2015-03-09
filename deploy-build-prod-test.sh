#!/bin/bash -e
API_HOST="mobile-rukmini1.nm.flipkart.com"
echo "Deploying build on "${API_HOST}
ssh ${API_HOST} "sudo apt-get update; sudo apt-get install rukmini=$1"
echo "Deployed build on "${API_HOST}
echo "Done"
echo
