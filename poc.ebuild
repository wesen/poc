
DESCRIPTION="lightweight multicast mp3 streamer"
HOMEPAGE="http://bl0rg.net/software/poc/"
SRC_URI="http://bl0rg.net/software/poc/${PN}-${PV}.tar.gz"
SLOT="0"
KEYWORDS="x86"
DEPEND=""
S="${WORKDIR}/${PN}-${PV}"

src_compile() {
		emake || die "emake failed"
}
src_install() {
	
	dobin pob-2250
	dobin pob-3119
	dobin pob-fec
	dobin poc-2250
	dobin poc-2250-ploss
	dobin poc-3119
	dobin poc-3119-ploss
	dobin poc-fec
	dobin poc-fec-ploss
	dobin poc-http
	dobin pogg-http

	dodoc README
	dodoc TODO
	dodoc radio.sh

	doman man/man1/pob-2250.1
	doman man/man1/pob-3119.1
	doman man/man1/pob-fec.1
	doman man/man1/poc-2250.1
	doman man/man1/poc-3119.1
	doman man/man1/poc-fec.1
	doman man/man1/poc-http.1
	doman man/man1/pogg-http.1
}
