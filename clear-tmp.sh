#!/bin/bash -e
for APINODE in 1 2 3 4 5 6 7 8 9 10 11 12
do
    API_HOST="cdn-img"${APINODE}".nm.flipkart.com"
    echo "Clearing temp on "${API_HOST}
    ssh ${API_HOST} "sudo /etc/init.d/rukmini cleartemp"
    echo "Cleared temp on "${API_HOST}
done
echo "Done"
echo
