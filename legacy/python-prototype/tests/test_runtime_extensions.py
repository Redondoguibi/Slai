import unittest
import test_compiler as harness


@unittest.skipUnless(harness.WINDOWS_X64, "Generated executables require Windows x64")
class RuntimeExtensionTests(unittest.TestCase):
    execute = harness.NativeTests.execute
    def test_catch_and_propagation(self):
        self.execute('func load(n):\n    if:\n        n < 0 => error NotFound\n    return n * 2\n'
                     'func wrap(n) => load(n)?\nx = 10\nx = wrap(-1) ?! err:\n    sys.log(err)\n    x = 7\n'
                     'sys.log(x)\nx = wrap(4) ?! err:\n    x = 0\nsys.log(x)\n', 'NotFound\n7\n8\n')

    def test_new_binding_requires_success_or_handler_assignment(self):
        self.execute('func fail():\n    error Failure\nx = fail() ?! err:\n    x = 42\nsys.log(x)\n', '42\n')

    def test_unhandled_error(self):
        self.execute('func fail():\n    error "Falha explícita"\nfail()\nsys.log("não executa")\n', 'Falha explícita\n', 1)

    def test_nested_catch_and_rethrow(self):
        self.execute('func fail():\n    error First\nfunc wrap():\n    fail() ?! err:\n        error Second\n'
                     'wrap() ?! err:\n    sys.log(err)\nsys.log("ok")\n', 'Second\nok\n')

    def test_spawn_await_and_multiple_arguments(self):
        self.execute('func sum(a,b,c,d,e,five):\n    sys.sleep(10)\n    return a+b+c+d+e+five\n'
                     'a = sys.spawn(sum,1,2,3,4,5,6)\nb = sys.spawn(sum,2,3,4,5,6,7)\n'
                     'sys.log(await a)\nsys.log(await b)\nsys.log(await a)\n', '21\n27\n21\n')

    def test_task_error_is_thread_local(self):
        self.execute('func fail():\n    error WorkerFailed\nfunc ok() => 42\n'
                     'a = sys.spawn(fail)\nb = sys.spawn(ok)\nx = 0\nx = await a ?! err:\n    sys.log(err)\n'
                     'sys.log(await b)\n', 'WorkerFailed\n42\n')

    def test_assertion_is_catchable(self):
        self.execute('sys.assert(true)\nsys.assert(false) ?! err:\n    sys.log("caught")\n', 'caught\n')

