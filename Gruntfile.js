module.exports = function(grunt) {

	grunt.file.defaultEncoding = 'utf8';
	grunt.loadNpmTasks('grunt-contrib-requirejs');
	grunt.loadNpmTasks('grunt-contrib-uglify');
	grunt.registerTask('default',['requirejs','uglify']);

	grunt.initConfig({
		requirejs: {
			compile: {
				options: {
					name: 'facedetection',
					baseUrl: 'build/src',
					// optimize : 'uglify',
					mainConfigFile:'build/src/build.js',
					logLevel: 0,
					findNestedDependencies: true,
					fileExclusionRegExp: /^\./,
					inlineText: true,
					out: "dist/facedetection.js"
				}
			}
		},
		uglify : {
			'dist/facedetection.min.js' : ['dist/facedetection.js']
		}
	});

};
