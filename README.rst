Golioth Reference Design Template
#################################

Overview
********

Use this repo as a template when beginning work on a new Golioth Reference
Design. It is set up as a standalone repository, with all Golioth features
implemented in basic form. Search the project for the word ``template`` and
``rd_template`` and update those occurrences with your reference design's name.

Local set up
************

Do not clone this repo using git. Zephyr's ``west`` meta tool should be used to
set up your local workspace.

Install the Python virtual environment (recommended)
====================================================

.. code-block:: console

   cd ~
   mkdir golioth-reference-design-template
   python -m venv golioth-reference-design-template/.venv
   source golioth-reference-design-template/.venv/bin/activate
   pip install wheel west

Use ``west`` to initialize and install
==================================

.. code-block:: console

   cd ~/golioth-reference-design-template
   west init -m git@github.com:golioth/reference-design-template.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt

This will also install the `golioth-zephyr-boards`_ definitions for the Golioth
Aludel-Mini.

Building the application
************************

Build Zephyr sample application for Golioth Aludel-Mini
(``aludel_mini_v1_sparkfun9160_ns``) from the top level of your project. After a
successful build you will see a new ``build`` directory. Note that any changes
(and git commmits) to the project itself will be inside the ``app`` folder. The
``build`` and ``deps`` directories being one level higher prevents the repo from
cataloging all of the changes to the dependencies and the build (so no
``.gitignore`` is needed)

During building, replace ``<your.semantic.version>`` to utilize the DFU
functionality on this Reference Design.

.. code-block:: console

   $ (.venv) west build -b aludel_mini_v1_sparkfun9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

Configure PSK-ID and PSK using the device shell based on your Golioth
credentials and reboot:

.. code-block:: console

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold

Golioth Features
****************

This app currently implements Over-the-Air (OTA) firmware updates, Settings
Service, Logging, and RPC. To adjust the delay between hello
messages, set a ``LOOP_DELAY_S`` key with a interger value (seconds) in the
Device Settings menu of the `Golioth Console`_.

.. _Golioth Console: https://console.golioth.io
.. _golioth-zephyr-boards: https://github.com/golioth/golioth-zephyr-boards
