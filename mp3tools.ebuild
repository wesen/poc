
DESCRIPTION="toolset for cutting mp3 files"
HOMEPAGE="http://bl0rg.net/software/poc/"
SRC_URI="http://bl0rg.net/software/poc/poc-${PV}.tar.gz"
SLOT="0"
KEYWORDS="x86"
DEPEND=""
S="${WORKDIR}/${PN}-${PV}"

src_compile() {
		emake || die "emake failed"
}
src_install() {
	
	dobin mp3cue
	dobin mp3cut
	dobin mp3length

	dobin README
	dodoc TODO

	doman man/man1/mp3cue.1
	doman man/man1/mp3cut.1
	doman man/man1/mp3length.1

}
