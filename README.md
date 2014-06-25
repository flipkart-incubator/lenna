# Rukmini - Image Delivery Service


## Dependencies
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