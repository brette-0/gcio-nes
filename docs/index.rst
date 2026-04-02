.. gcio-nes documentation master file, created by
   sphinx-quickstart on Thu Apr  2 23:51:45 2026.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

gcio-nes
======================

``gcio-nes`` is a project bringing GameCube input to the NES/Famicom through an Atmel AtMega8 microcontroller adaptor
as third party successor to the 'raphnet' GameCube to NES Adaptor using the same technology.

The Vision
----------

1. Full report and Backwards Compatibility
    a. The ability to report all the information from the controller relevant to the system its connected to.

    b. Triple buffered to allow for instant controller reading formatted to imitate a NES, then SNES controller

    c. 100% Backwards compatibility support for games that correctly use a standard American NES/SNES Controller.
2. Console Reprogrammable behavior
    a. Sticks can report raw deltas to centre *or* a combination of angle with magnitude.

    b. Stick Sync options, permitting L to transfer to C or C to L on the adaptor..

    c. Option for creating an angle with magnitude for either or both of L and C stick from d pad inputs.

    d. Adaptor input skipping if not all inputs are required.

    e. Built-in input bitflips for runtime input conversion.


How it works
------------
Every ``6ms`` the adaptor commands the controller for its ID, fetching the necessary information to correct its output
(such as stick neutral location). Then immediately sends a command for the controller to get the current state. While
The controller is generating the information for the adaptor to receives it modified its internal state to correctly
receive the inputs from the controller and preparing itself for reads from the controller.

Given the code:

.. code-block::

    lda #$01
    sta buttons     ; (user defined memory location)
    sta $4016       ; as long as the the reads on $4016 occurred while OUT was deasserted (typical)
                    ; this does nothing to the adaptor as its polls the controller continuously
    lda #$00
    sta $4016       ; since no reads were performed to $4016 while OUT was asserted, this either:
                    ; (1) sets the adaptor task to 'compatability report' mode
                    ; (0) does nothing, as its latched into an advanced task

    :   lda $4016   ; if not latched into an advanced task, will read bits like a NES Controller
                    ; otherwise, will return zeroes until the task is exhausted.
        lsr a
        rol buttons
        bcc -

.. note::
    If the adaptor is latched into an advanced task as its being used in a game that does not use the feature. It will
    at most return empty inputs for eight reads which typically is eight frames. This is because reads are used to
    accept bits to build a buffer when latched. Once the task is unlatched it will set the task index to ``0`` (legacy)
    in which it will increment the task again on each ``$4016`` read when ``OUT`` is asserted.

    Nearly all games will not read while ``OUT`` is asserted which will guarantee after 8 cycles the backwards
    compatibility mode will be selected.


But to use the full capability, the correct report code is:

.. code-block::

    lda #$01
    sta buttons
    sta $4016
    bit $4016
    lsr a
    sta $4016

    ldx #$08
    :   dex
        bne ++
    :   lda $4016
        lsr
        rol buttons, x
        bcc -
        clc
        bcs --
    :

This code reports ``64`` bits of data from the controller such as long as the mask is set up properly. It knows to use
the full capability mode as the ``bit $4016`` performs a read while ``OUT`` is asserted increment the task index to
``1`` which is responsible for full capacity responses.