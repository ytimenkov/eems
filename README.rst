EEMS
====

A media server for home NAS.

Build instructions
------------------

Use `Conan <https://conan.io>`_ for dependencies.

.. code-block:: bash

    # Install Conan
    $ pip3 install --user conan

    # Autodetect system settings
    $ conan profile new default --detect

    # Install dependencies (debug build)
    $ conan install . -if build/dev -pr:h default -pr:h conan/dev -pr:b default --build missing

    # Configure CMake
    $ conan build . --configure -bf build/dev

    # Build in build/dev

