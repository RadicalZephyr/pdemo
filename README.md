Introduction
============

PDIP (Programmed Dialogue with Interactive programs) can be installed
in three ways:

1. Installation from the sources
2. Installation from the DEB binary package
3. Installation from the RPM binary package

From the sources, you can generate a DEB or binary package:

4. Generation of a DEB binary package
5. Generation of a RPM binary package

You can generate a zipped tar file of the sources:

6. Generation of a zipped tar file


1. Installation from the sources
--------------------------------

  * The installation from the sources supposes that cmake is installed
    on your Linux system

  * Unpack the tar compressed file pdip-xxx.tgz into a directory. This
  will create a sub-directory called pdip-xxx with the source files of
  the program:

    $ tar xvfz pdip-xxx.tgz

  * Go into the newly created directory:

    $ cd pdip-xxx

  * Make sure the file pdip_install.sh has the execute permission:

    $ chmod +x pdip_install.sh

  * Launch the script pdip_install.sh to get the help:

    $ ./pdip_install.sh -h

    Usage: pdip_install.sh [-d install_root_dir] [-P DEB | RPM] [-B] [-I] [-A] [-h]

             -d : Installation directory (default: /usr/local)
             -P : Generate a DEB or RPM package
             -B : Build the software
             -I : Install the software
             -A : Generate an archive of the software (sources)
             -h : this help

  * Under root identity, launch the installation by passing '-I' and optionnaly '-d' to
  specify an installation directory different than '/usr/local':
  For example, for an installation in '/usr/local', type:

    $ sudo ./pdip_install.sh -I

  For an installation in '/usr', type;

    $ sudo ./pdip_install.sh -I -d /usr


  * If your PATH variable is correctly set, PDIP help can be displayed:

    $ pdip --help

  * If your MANPATH variable is correctly set, PDIP's online manual can
be displayed:

    $ man pdip


2. Installation of the binaries from the DEB package
----------------------------------------------------

The files are installed via the command:

    $ sudo dpkg -i pdip-xxx.deb


3. Installation of the binaries from the RPM package
----------------------------------------------------

The files are installed via the command:

    $ sudo rpm -i pdip-xxx.rpm


4. Generation of a DEB binary package
-------------------------------------

  * The installation from the sources supposes that cmake is installed
    on your Linux system

  * Unpack the tar compressed file pdip-xxx.tgz into a directory. This
    will create a sub-directory called pdip-xxx with the source files
    of the program:

    $ tar xvfz pdip-xxx.tgz

  * Go into the newly created directory:

    $ cd pdip-xxx

  * Make sure the file pdip_install.sh has the execute permission:

    $ chmod +x pdip_install.sh

  * Launch the script pdip_install.sh to get the help:

    $ ./pdip_install.sh -h

    Usage: pdip_install.sh [-d install_root_dir] [-P DEB | RPM] [-B] [-I] [-A] [-h]

             -d : Installation directory (default: /usr/local)
             -P : Generate a DEB or RPM package
             -B : Build the software
             -I : Install the software
             -A : Generate an archive of the software (sources)
             -h : this help

  * Under root identity, launch the installation by passing '-P DEB'
  and optionnaly '-d' to specify an installation directory different
  than '/usr/local': For example, for a package which will be
  installed in '/usr/local', type:

    $ sudo ./pdip_install.sh -P DEB

  For an installation in '/usr', type;

    $ sudo ./pdip_install.sh -P DEB -d /usr



5. Generation of a RPM binary package
-------------------------------------

  * The installation from the sources supposes that cmake is installed
    on your Linux system

  * Unpack the tar compressed file pdip-xxx.tgz into a directory. This
    will create a sub-directory called pdip-xxx with the source files
    of the program:

    $ tar xvfz pdip-xxx.tgz

  * Go into the newly created directory:

    $ cd pdip-xxx

  * Make sure the file pdip_install.sh has the execute permission:

    $ chmod +x pdip_install.sh

  * Launch the script pdip_install.sh to get the help:

    $ ./pdip_install.sh -h

    Usage: pdip_install.sh [-d install_root_dir] [-P DEB | RPM] [-B] [-I] [-A] [-h]

             -d : Installation directory (default: /usr/local)
             -P : Generate a DEB or RPM package
             -B : Build the software
             -I : Install the software
             -A : Generate an archive of the software (sources)
             -h : this help

  * Under root identity, launch the installation by passing '-P RPM'
  and optionnaly '-d' to specify an installation directory different
  than '/usr/local': For example, for a package which will be
  installed in '/usr/local', type:

    $ sudo ./pdip_install.sh -P RPM

  For an installation in '/usr', type;

    $ sudo ./pdip_install.sh -P RPM -d /usr



6. Generation of a zipped tar file
----------------------------------

  * The installation from the sources supposes that cmake is installed
    on your Linux system

  * Unpack the tar compressed file pdip-xxx.tgz into a directory. This
    will create a sub-directory called pdip-xxx with the source files
    of the program:

    $ tar xvfz pdip-xxx.tgz

  * Go into the newly created directory:

    $ cd pdip-xxx

  * Make sure the file pdip_install.sh has the execute permission:

    $ chmod +x pdip_install.sh

  * Launch the script pdip_install.sh to get the help:

    $ ./pdip_install.sh -h

    Usage: pdip_install.sh [-d install_root_dir] [-P DEB | RPM] [-B] [-I] [-A] [-h]

             -d : Installation directory (default: /usr/local)
             -P : Generate a DEB or RPM package
             -B : Build the software
             -I : Install the software
             -A : Generate an archive of the software (sources)
             -h : this help

  * Under root identity, launch the installation by passing '-A':

    $ sudo ./pdip_install.sh -A
