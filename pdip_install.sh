#!/bin/bash
#
# Simple script to install PDIP
#
#  Copyright (C) 2007,2008 Rachid Koucha <rachid dot koucha at free dot fr>
#
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


TMPDIR="/tmp/pdip_$$"

PDIP_SOURCES="ChangeLog.txt     \
              CMakeLists.txt    \
              FindGZIP.cmake    \
              pdip_chown.cmake  \
              pdip.c            \
              pdip_deb.ctrl     \
              pdip_deb.postinst \
              pdip_en.1         \
              pdip_fr.1         \
              pdip_install.sh   \
              README.txt        \
              COPYING           \
              AUTHORS           \
              index.html        \
              config.h.cmake    \
              version.cmake     \
              glmf_logo.gif"


cleanup()
{
  if [ -d ${TMPDIR} ]
  then
    rm -rf ${TMPDIR}
  fi
}

trap 'cleanup' HUP INT EXIT TERM QUIT


# Default values
INST_DIR=/usr/local
BUILD_IT=0
INSTALL_IT=0
DEB_PACKAGE_IT=NO
RPM_PACKAGE_IT=NO
ARCHIVE_IT=0


# User manual
help()
{
{
echo
echo Usage: `basename $1` "[-d install_root_dir] [-B] [-I] [-A] [-P RPM|DEB] [-h]"
echo
echo "             -d : Installation directory (default: ${INST_DIR})"
echo '             -P : Generate a DEB or RPM package'
echo '             -B : Build the software'
echo '             -I : Install the software'
echo '             -A : Generate an archive of the software (sources)' 
echo '             -h : this help'
} 1>&2
}



# If no arguments ==> Display help
if [ $# -eq 0 ]
then
  help $0
  exit 1
fi

# Make sure that we are running in the source directory
if [ ! -f pdip.c ]
then
  echo This script must be run in the source directory of PDIP >&2
  exit 1
fi

# Parse the command line
while getopts d:P:BIAh arg
do
  case ${arg} in
    d) INST_DIR=${OPTARG};;
    P) if [ ${OPTARG} = "DEB" ]
       then DEB_PACKAGE_IT=YES
       elif [ ${OPTARG} = "RPM" ]
       then RPM_PACKAGE_IT=YES
       else echo Unknown package type \'${OPTARG}\' >&2
            help $0
            exit 1
       fi;;
    B) BUILD_IT=1;;
    I) INSTALL_IT=1;;
    A) ARCHIVE_IT=1;;
    h) help $0
       exit 0;;
    *) help $0
       exit 1;;
  esac
done

shift $((${OPTIND} - 1))

# Check the arguments
if [ -n "$1" ]
then
  echo Too many arguments >&2
  help $0
  exit 1
fi

# Make sure that cmake is installed if build or installation or packaging is requested
if [ ${BUILD_IT} -eq 1 -o ${INSTALL_IT} -eq 1 -o ${DEB_PACKAGE_IT} != "NO" -o ${RPM_PACKAGE_IT} != "NO" ]
then
  which cmake > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo To be able to compile/install PDIP, you must install cmake and/or update the PATH variable >&2
    exit 1
  fi

  # Launch cmake
  echo Configuring PDIP installation in ${INST_DIR}...
  cmake . -DCMAKE_INSTALL_PREFIX=${INST_DIR}
fi

# If archive is requested
if [ ${ARCHIVE_IT} -eq 1 ]
then

  which tar > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo "To be able to generate a PDIP archive, you must install 'tar' and/or update the PATH variable" >&2
    exit 1
  fi

  # Launch the build
  make

  # Get PDIP's version
  PDIP_VERSION=`./pdip -V | cut -d' ' -f3`
  if [ -z "${PDIP_VERSION}" ]
  then
    echo Something went wrong while building PDIP >&2
    exit 1
  fi

  ARCHIVE_DIR=pdip-${PDIP_VERSION}
  ARCHIVE_NAME=${ARCHIVE_DIR}.tgz
  mkdir -p ${TMPDIR}/${ARCHIVE_DIR}
  cp -R ${PDIP_SOURCES} ${TMPDIR}/${ARCHIVE_DIR}
  echo Building archive ${ARCHIVE_NAME}...
  tar cvfz ${ARCHIVE_NAME} -C ${TMPDIR} ${ARCHIVE_DIR} > /dev/null 2>&1

  rm -rf ${TMPDIR}/${ARCHIVE_DIR}
fi

# If build is requested
if [ ${BUILD_IT} -eq 1 ]
then
  make
fi

# If installation is requested
if [ ${INSTALL_IT} -eq 1 ]
then
  make install
fi


if [ ${DEB_PACKAGE_IT} != "NO" -o ${RPM_PACKAGE_IT} != "NO" ]
then

  # Make sure that DPKG is installed
  which dpkg > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo "To be able to generate a DEB or RPM PDIP package, you must install 'dpkg' and/or update the PATH variable" >&2
    exit 1
  fi

  # Make sure that ALIEN is installed
  if [ ${RPM_PACKAGE_IT} != "NO" ]
  then
    which alien > /dev/null 2>&1
    if [ $? -ne 0 ]
    then
      echo "To be able to generate a RPM PDIP package, you must install 'alien' and/or update the PATH variable" >&2
      exit 1
    fi
  fi

  # Make sure that sed is installed
  which sed > /dev/null 2>&1
  if [ $? -ne 0 ]
  then
    echo "To be able to generate a DEB PDIP pakage, you must install 'sed' and/or update the PATH variable" >&2
    exit 1
  fi

  # Launch the build
  make

  # Get PDIP's version
  PDIP_VERSION=`./pdip -V | cut -d' ' -f3`
  if [ -z "${PDIP_VERSION}" ]
  then
    echo Something went wrong while building PDIP >&2
    exit 1
  fi

  # Reproduce the installation tree in the temporary directory
  if [ ${DEB_PACKAGE_IT} != "NO" ]
  then
    echo Making the DEB package...
  fi
  mkdir -p ${TMPDIR}/${INST_DIR}
  mkdir -p ${TMPDIR}/${INST_DIR}/bin
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/man1
  mkdir -p ${TMPDIR}/${INST_DIR}/share/man/fr/man1
  mkdir -p ${TMPDIR}/DEBIAN
  cp pdip ${TMPDIR}/${INST_DIR}/bin
  cp pdip_en.1.gz ${TMPDIR}/${INST_DIR}/share/man/man1/pdip.1.gz
  cp pdip_fr.1.gz ${TMPDIR}/${INST_DIR}/share/man/fr/man1/pdip.1.gz
  cat pdip_deb.ctrl | sed "s/^Version:.*$/Version: ${PDIP_VERSION}/g" > ${TMPDIR}/DEBIAN/control
  cat pdip_deb.postinst | sed "s%^INSTALL_PREFIX=.*$%INSTALL_PREFIX=${INST_DIR}%g" > ${TMPDIR}/DEBIAN/postinst
  chmod +x ${TMPDIR}/DEBIAN/postinst

  # Generate the DEB package
  dpkg -b ${TMPDIR} . >/dev/null 2>&1

  # Generate the RPM package if requested
  if [ ${RPM_PACKAGE_IT} != "NO" ]
  then
    echo Making the RPM package...
    alien --to-rpm pdip_${PDIP_VERSION}_ia64.deb --scripts
  fi

fi

#
# Later I will make the RPM package without using alien as done above
#

# If RPM package is requested
#if [ ${PACKAGE_IT} = "RPM" ]
#then
#  echo Sorry, the RPM packaging is not yet supported. >&2
#  echo The designer didn\'t find time to implement it but it will be done soonly... >&2
#  exit 1
#fi


