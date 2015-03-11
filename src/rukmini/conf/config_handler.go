package conf

import "github.com/astaxie/beego"


const (
	FK_CDN_HYSTRIX_COMMAND = "fk_cdn"
)
func GetFkCdnTimeout() int64 {
	timeout, _ := beego.AppConfig.Int64("fkcdn_timeout")
	return timeout
}
