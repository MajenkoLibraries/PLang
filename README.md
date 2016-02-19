PLang
=====

Simple PIC32 scripting language.
--------------------------------

This language is designed to be incredibly simple to write. Scripts are loaded from an
SD card as plain text and compiled into a structured tree of data to then be executed.

Multiple PLang engines can be instantiated simultaneously and naturally thread together.

PLang is event driven where events are currently limited to digital input state changes.

The structure of a PLang file is:

    Initialization parameters - defining variables, events, etc.

    init:  op-code parameter list
           ... etc ...
           return

    label: op-code parameter list
           op-code parameter list
           ... etc ...
           return

    ... and so on ...


The initial configuration block consists of a set of `def` and `link` statements.

* `def` defines a variable and gives it an initial value.

    def variable number
    def myVariable 32

* `link` connects a digital input to a function.

    link input direction label
    link 7 rising myCodeBlock
    link myVariable falling myOtherCodeBlock

The `init:` block is always called first to do any configuration tasks you may need.  From
your other code blocks are called whenever one of the events defined by `link` is detected.

Op-codes are:

* NOP

Do nothing. Useful as a placeholder with a label (labels can only go before an op-code and
never by themselves).

* MODE pin in|out [pullup]

Set the pin mode of a digial IO pin to either input or output. If the mode is `in` the `pullup`
flag will enable the pullup resistor if one is available.

* IF item A operator item B opcode...

Calls operator on the two items, and if the result is true the associated opcode (and its
parameters) are executed.  Operators include:

    LT - A is less than B
    LE - A is less than or equal to B
    EQ - A and B are equal
    GE - A is greater than or equal to B
    GT - A is greater than B
    READS - The value read from digital pin A is equal to B

* CALL label

Calls the routine at the specified label. When the routine reaches a RETURN opcode the execution
continues from the next op-code.

* GOTO label

Execution jumps to the specified label.

* PLAY file

Not implemented yet.  Eventually it will play the specified WAV file through an audio device.

* PRINT item

Display item on the serial port.  If the item is enclosed in "" it will treat it as a string.
If the item is numeric it will treat it as a number.  Otherwise it tries to match it against a
variable name.

* PRINTLN item

The same as PRINT but a new-line is appended to the output.

* SET variable value

Gives a variable a new value.  The new value may either be a numeric literal or the contents
of another variable.

* DISPLAY item

Display the contents of the variable, or numeric literal, specified on some attached display
device (not implemented yet).

* DELAY ms

Delay for the specified number of milliseconds.

* DEC variable

Decrement the given variable by 1.

* INC variable

Increment the given variable by 1.

* RETURN

Return from a function when the function has been accessed using CALL or an event.

Example
-------

    # This links the two buttons on a chipKIT WF32 to a counter variable. One button
    # increases the variable, the other decreases it. The value is then printed on
    # the serial console.

    # Define variables for the button pin numbers to make it easier to read
    def down 65
    def up 66

    # The counter variable - starts at 0
    def val 0
      
    # Link the two buttons to two functions
    link down rising countdown
    link up rising countup

    # The init routine sets the pin modes for the two buttons.
    init:         mode down in
                  mode up in
                  println val
                  return

    # Decrement and display the value
    countdown:    dec val
                  prinln val
                  return

    # Increment and display the value
    countup:      inc val
                  println val
                  return


