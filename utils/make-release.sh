#!/bin/sh

SVN_REP="svn://bl0rg.net/poc"
RELEASE_TAG=$1
RELEASE_TAG_URL="${SVN_REP}/tags/${RELEASE_TAG}"
TRUNK_URL="${SVN_REP}/trunk"

CDROMDIR="poc-${RELEASE_TAG}"

if [ -z "${RELEASE_TAG}" ]; then
   echo "Please give a release number"
   exit 1
fi

echo "Exporting subversion repository"
svn export "${TRUNK_URL}" "${CDROMDIR}" >/dev/null

rm -rf "${CDROMDIR}/tex"
rm -rf "${CDROMDIR}/debian"
# Packages fuer andere dists kann man ruhig drinnen lassen
#rm -rf "${CDROMDIR}/poc.spec"
#rm -rf "${CDROMDIR}/*.ebuild"

tar zcvf "poc-${RELEASE_TAG}.tar.gz" "${CDROMDIR}"

rm -rf "${CDROMDIR}"
