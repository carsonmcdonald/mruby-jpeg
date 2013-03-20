require 'tmpdir'

MRuby::Gem::Specification.new('mruby-jpeg') do |spec|
  spec.license = 'MIT'
  spec.authors = 'Carson McDonald'
  spec.version = '0.1.0'
  spec.description = 'mruby wrapper for libjpeg.'
  spec.homepage = 'https://github.com/carsonmcdonald/mruby-jpeg'

  spec.linker.libraries << 'jpeg'

  FileUtils.cp_r(File.expand_path(File.dirname(__FILE__)) + '/test/test.jpg', Dir::tmpdir)

  spec.test_args = {'test_jpeg' => Dir::tmpdir + '/test.jpg'}
end
