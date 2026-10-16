# BLE C Coding Style

These formatting rules apply to the BLE library, examples, and BLE Mesh code.

## Unbraced `if` statements

Keep a single-line condition on its own line and put the controlled statement on the next indented line:

```c
if (!conn || !psm || psm > 0x00ffu)
    return 0;
```

When the condition wraps across lines, put the closing parenthesis and the controlled statement together on a line of their own:

```c
if (!conn->ops.send_pdu(conn->ops.context, ch->remote_cid,
                        frame, used)
) return 0;
```

Apply this layout to unbraced `if` statements with other single statements as well. Leave braced `if` blocks unchanged.

## Space between `if` statements

Put a blank line before each `if` statement, except when it is the first statement in a method or is nested inside another `if`. This applies even when the preceding `if` is one line.

Separate loops from the statements before and after them with a blank line. Do not put a blank line between a loop header and its body.

Separate setup statements, including local declarations and assignments, from the following control-flow statement with a blank line.

Separate an early exit from the following comment, declaration, or independent statement with a blank line.
