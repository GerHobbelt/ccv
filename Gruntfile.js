module.exports = function(grunt) {

	grunt.file.defaultEncoding = 'utf8';
	grunt.loadNpmTasks('grunt-contrib-requirejs');
	grunt.registerTask('default',['requirejs']);

	grunt.initConfig({
		requirejs: {
			compile: {
				options: {
					name: 'facedetection',
					baseUrl: 'build/src',
					optimize : 'uglify',
					mainConfigFile:'build/src/build.js',
					logLevel: 0,
					findNestedDependencies: true,
					fileExclusionRegExp: /^\./,
					inlineText: true,
					out: "dist/facedetection.js"
				}
			}
		}
	});

};
