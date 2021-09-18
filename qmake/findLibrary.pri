load(configure)

QMAKE_CONFIG_TESTS_DIR = $$PWD/config.tests
CONFIG += recheck

defineTest(findLibrary) {
  # Name of the library when printing QMake messages
  prettyName = $$1

  # List of test names to execute in order
  # At least one must succeed if library isRequired, all may fail if !isRequired
  # If none succeed and library isRequired, errors build with appropriate message
  testNames_var = $$2
  testNames = $$eval($$testNames_var)

  # If this library is required by default (i.e. in general or for the target platform)
  isRequired = $$3

  if(!isEmpty(isRequired):$$isRequired): dependency = "required"
  else: dependency = "detected"

  for(TEST, testNames) {
    qtCompileTest($$TEST)
    if(config_$$TEST) {
      message("[$$dependency] $$prettyName found & enabled")
      return(true)
    }
  }

  !equals(dependency, "detected") {
    error("$$prettyName support is $$dependency but all tests failed! Check $$_PRO_FILE_PWD_/config.log for more details!")
    return(false)
  }
}
