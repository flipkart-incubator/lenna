# Lenna - Image Delivery Service

Responsive images are landing soon and many organizations are looking for ways to resize images.
Lenna is used in Flipkart for resizing images on the fly. The high CPU requirement due to resizing requires images to be cached on the CDN.
Lenna can resize and alter the quality of the image. Currently support for jpeg, webp, png and gif has been provided.

The service makes remote calls to the storage system which hosts the original image which needs to be resized. The remote network call is wrapped around hystrix which enables a better insight into the system.

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

### Setup Development and Production Environment
* Clone the git repo: `git clone --recursive git@github.com:flipkart-incubator/lenna.git`
* Change to root project directory
* Set Environment Variables
  `export GOROOT=`
  `export GOPATH=``pwd``
* Build
  `cd src/lenna`
  `go build`
* Run
  `go run main.go`

### Setup Vim
Best and easiest way to setup an editor for golang is vim.
Please follow the following step to setup go-specific-feature in your vim https://github.com/fatih/vim-go

### Setup IntelliJ
* Install golang plugin - 0.9.15+
* Setup Run Configuration for `Go Application`
* Set environment variables `PATH=/usr/local/bin:/usr/bin:$PATH;GOPATH=<product_base_directory>/Lenna;GOROOT=;`
* Set script to run: `<product_base_directory>/src/Lenna/main.go`

### Setup hot code replace (bee tool)
* Set `GOPATH` to go home: `export GOPATH=/usr/local/go`
* Install bee tool: `go get github.com/beego/bee`
* bee tool documentation: `http://beego.me/docs/install/bee.md`

### Known Issues
* Lenna may not compile on ubuntu as the C bindings of chai2010 and nfnt conflicts with the system libraries. However, Lenna has been successfully tested on Debian and Mac.
