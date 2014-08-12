#!/bin/bash -e
for APINODE in 2 3 4 5 11 12
do
    API_HOST="cdn-img"${APINODE}".nm.flipkart.com"
    echo ${API_HOST}
    curl -XPOST ${API_HOST}":8081/bir" || true
done
for APINODE in 1 2 3 4
do
    API_HOST="rukmini"${APINODE}".nm.flipkart.com"
    echo ${API_HOST}
    curl -XPOST ${API_HOST}":8081/bir" || true
done
echo "Done"
echo
