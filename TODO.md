# TODO

- Allow for custom temp directory name (example of usage: downloading of games updates from a launcher).
- Allow for custom changelog path for current installation.
- Allow for NOT automatic restart.
- Rewrite tests for more general purpose.

Use `QTest::qWaitfor` instead of `while` and `processEvents` (same thing but a bit cleaner).

```c++
if (!QTest::qWaitFor([&done]() { return done; }, 1000)) {
  QFAIL("Too late.");
}
```
