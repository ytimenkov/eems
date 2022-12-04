EEMS
====

A media server for home NAS.

Build instructions
------------------

Use `Conan <https://conan.io>`_ for dependencies.

.. code-block:: bash

    # Install Conan
    $ pipx install conan

    # Autodetect system settings
    $ conan profile new default --detect

    # Install dependencies (debug build)
    $ conan install . -if build -pr:h default -pr:h conan/dev -pr:b default --build missing

    # Configure CMake
    $ bash -c " . build/clang/generators/conanbuild.sh && cmake --preset debug"

    # Build in build/Debug
    $ bash -c " . build/clang/generators/conanbuild.sh && cmake --build --preset debug"
