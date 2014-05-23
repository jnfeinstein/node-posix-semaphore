var Cluster = require('cluster');
var Sem = require('../src/main');

module.exports = {
  tearDown: function(callback) {
    if (this.semaphore) {
      this.semaphore.delete();
    }
    callback();
  },
  testSemaphoreConstructor: function(test) {
    test.throws(
      function() { new Sem.Semaphore(); },
      Error,
      "Semaphore::New -> Requires a key arg");
    test.done();
  }
}
