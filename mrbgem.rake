MRuby::Gem::Specification.new('mruby-jpeg') do |spec|
  spec.license = 'MIT'
  spec.authors = 'Carson McDonald'

  spec.linker.libraries << 'jpeg'
end
