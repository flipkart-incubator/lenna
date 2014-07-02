# Rukmini - Image Delivery Service

## Supported Endpoints
* Health check: `GET /status`
* OOR: `POST /oor`
* BIR: `POST /bit`
* Resize: `GET /{source prefix}/{width}/{height}/{relative path from source}?[q={quality}]`

## Example Resize

`/image/500/500/tablet/3/u/g/lenovo-yoga-10-b8000-original-imadryghr3d7tr6e.jpeg`

## Production Instance
* Internal VIP: `rukmini.flixcart.vip.nm.flipkart.com`
* CDN Origin CName: `rukmini.flixcart.com`
* CDN Edge CNames: `rukmini1.flixcart.com, rukmini2.flixcart.com, rukmini3.flixcart.com`

## Runtime Dependencies
* [go 1.3](http://golang.org/)
* [beego](http://beego.me/)
* [libmagickwand5](https://packages.debian.org/wheezy/libmagickwand5)
* [libmagickwand-dev](https://packages.debian.org/wheezy/libmagickwand-dev)


## Development Setup Instructions on OSX
### Requirements
* brew [Install Instructions](http://brew.sh/)
* git `brew install git`
* XCode Command Line tools `xcode-select --install`
* go [Install Instructions](http://golang.org/doc/install)
* imagemagick `brew install imagemagick`
* magickwand `brew install magickwand`
* Preferred IDE: IntelliJ 13.1.3+ (using vim/emacs/atom/mate/sublime is fine too)

### Setup Development Environment
* Clone the git repo: `git clone --recursive git@github.com:Flipkart/rukmini.git`
* Change to root project directory
* Set Environment Variables
  `export GOROOT=`
  `export GOPATH=``pwd``
* Build
  `cd src/rukmini`
  `go build`
* Run
  `go run main.go`

### Setup IntelliJ
* Install golang plugin - 0.9.15+
* Setup Run Configuration for `Go Application`
* Set environment variables `PATH=/usr/local/bin:/usr/bin:$PATH;GOPATH=<product_base_directory>/rukmini;GOROOT=;`
* Set script to run: `<product_base_directory>/src/rukmini/main.go`

### Setup hot code replace (bee tool)
* Set `GOPATH` to go home: `export GOPATH=/usr/local/go`
* Install bee tool: `go get github.com/beego/bee`
* bee tool documentation: `http://beego.me/docs/install/bee.md`