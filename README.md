# corecraft

corecraft is a World of Wacraft 2.4.3 (The Burning Crusade) Server based on MaNGOS and the work of [ccshiro](https://github.com/ccshiro/).
This project is a fork of the [ccshiro/corecraft](https://github.com/ccshiro/corecraft) repository.

The goal of this fork is to make it compile on modern linux systems, providing install instructions and updated directory structures.
**Do not** expect windows support or any active development here. This fork exists only out of my curiosity.

## Install Guide

### Preparation

ArchLinux is used in this example. For debian based sytems use `apt` instead of `pacman` and replace the
packages with the corresponding ones of your distribution.

    pacman -S base-devel boost cmake git sparsehash
    git clone https://github.com/shagu/corecraft.git

### Compile

Change `make -j8` to the number of CPUs/Threads you have. The higher the number, the more
threads will be utilizied to compile the core.

    mkdir build; cd build
    cmake ../server -DTOOLS=1 -DCMAKE_INSTALL_PREFIX=install
    make -j8
    make install

### Game Files

In order to make the server startup, it is required to extract client files first.
Copy the extractors to your TBC Gameclient (2.4.3) and execute then. Move the results
back to your server directory.

    cd install/bin/
    cp dbc_map_extractor mmap_generator vmap4_assembler vmap4_extractor ~/games/tbc
    cd ~/games/tbc

    mkdir vmaps

    ./dbc_map_extractor
    ./vmap4_extractor
    ./vmap4_assembler Buildings/ vmaps
    ./mmap_generator

    cd -
    cp -r ~/games/tbc/{dbc,maps,vmaps,mmaps} ..

### Database

	pacman -S mariadb
	mariadb-install-db --user=mysql --basedir=/usr --datadir=/var/lib/mysql
	systemctl start mariadb
	mysql_secure_installation

	mysql -uroot -p < server/sql/create_mysql.sql

	mysql -umangos -p"mangos" characters < database/characters.sql
	mysql -umangos -p"mangos" characters < database/characters_db_version.sql

	mysql -umangos -p"mangos" realmd < database/realmd.sql
	mysql -umangos -p"mangos" realmd < database/realmd_db_version.sql

	mysql -umangos -p"mangos" mangos < database/world.sql
	mysql -umangos -p"mangos" mangos < database/scriptdev2.sql


#### Create Realm/Account

    mysql -umangos -p"mangos" realmd
        INSERT INTO `realmd`.`account` (`id`, `username`, `sha_pass_hash`, `gmlevel`, `v`, `s`) VALUES ('1', 'ADMINISTRATOR', 'a34b29541b87b7e4823683ce6c7bf6ae68beaaac', '3', '0', '0');
        INSERT INTO `realmlist` (`id`, `name`, `address`, `port`, `icon`, `realmflags`, `timezone`, `allowedSecurityLevel`, `population`, `realmbuilds`, `alliance_online`, `horde_online`) VALUES (1, 'corecraft', '127.0.0.1', 8085, 0, 0, 1, 0, 0, '', 0, 0);
        exit;

### Startup

Move to the build/install directory and make sure all client exports are there.
The directory should look like this:

    $ ls -1
      bin
      dbc
      etc
      maps
      mmaps
      run-mangos
      vmaps

Create the required config files and modify them to your needs:

    mv etc/mangosd.conf.dist etc/mangosd.conf
    mv etc/realmd.conf.dist etc/realmd.conf
    vim etc/realmd.conf etc/mangosd.conf

Now launch `./bin/realmd` and `./bin/mangosd`.


# Archive

**Original Readme (author: ccshiro):**

    This is the codebase of corecraft as was when the project stopped.
    Its's filled with problems and bad code, and should not be used in a production server.
    A lot of effort was put into the content, so we're putting it up for downloads
    if anyone is curious to check it out. The git history has been stripped as it contained
    private information.

    The server has been tested on Ubuntu compilled with GCC and Clang. It uses various
    C++11/14 features and might not compile on Windows with MSVC.

    To run the server:
    Put scriptdev2 in bindings folder.
    Run CMake on the server directory, resolve any dependencies you're missing.
    Compile & install with make.
    Generate data files with the tools (-DTOOLS=1 for CMake).
    Set up the database, found in database/.
    Fix the config files.
    Run mangosd and realmd.
    (setup_test.txt might be useful, its the steps we took to set up our many tests)
