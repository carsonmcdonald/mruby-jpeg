require 'tmpdir'

MRuby::Gem::Specification.new('mruby-jpeg') do |spec|
  spec.license = 'MIT'
  spec.authors = 'Carson McDonald'

  spec.linker.libraries << 'jpeg'

  FileUtils.cp_r(File.expand_path(File.dirname(__FILE__)) + '/test/test.jpg', Dir::tmpdir)

  spec.test_args = {'test_jpeg' => Dir::tmpdir + '/test.jpg'}
end
