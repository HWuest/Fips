openssl genrsa -out "Fips.key" 2048
openssl req -new -key "Fips.key" -out "Fips.csr"
openssl x509 -req -days 1095 -in "Fips.csr" -signkey "Fips.key" -out "Fips.crt"

Password DeafBlindFips!
