# BLE C Coding Style

These formatting rules apply to the BLE library, examples, and BLE Mesh code.

## Unbraced `if` statements

For a single-line condition, put the controlled statement on the next indented line by default:

```c
if (!conn || !psm || psm > 0x00ffu)
    return 0;
```

If the complete unbraced `if` statement fits within 40 characters, it may be written on one line:

```c
if (!id) return 0;
```

When the condition wraps across lines, put the closing parenthesis and the controlled statement together on a line of their own:

```c
if (!conn->ops.send_pdu(conn->ops.context, ch->remote_cid,
                        frame, used)
) return 0;
```

Apply this layout to unbraced `if` statements with other single statements as well. Leave braced `if` blocks unchanged.

## Braced `else` clauses

Put `else` on a new line after the closing brace. Apply the same layout to `else if`:

```c
if (ready) {
    start();
}
else if (retry) {
    try_again();
}
else {
    stop();
}
```

## Space between `if` statements

Put a blank line before each `if` statement, except when it is the first statement in a method or directly nested inside another `if`. This applies even when the preceding `if` is one line. A declaration or other setup statement before an `if` still requires a blank line, including inside a block or switch case.

Separate loops from the statements before and after them with a blank line. Do not put a blank line between a loop header and its body.

Separate setup statements, including local declarations and assignments, from the following control-flow statement with a blank line.

After an early exit (`return`, `break`, `continue`, or `goto`), put a blank line before the next independent statement or comment, regardless of the exit expression or whether an unbraced `if` controls it.

Separate a completed conditional block from a following declaration, loop, or independent statement with a blank line.
