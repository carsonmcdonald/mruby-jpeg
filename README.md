mruby-jpeg
==========

*Note: This is a work in progress, only supports reads right now*

Support for reading JPEGs in mruby using libjpeg.

## Installation

In the mruby directory add the following to your build config:

```ruby
MRuby::Build.new do |conf|

  conf.gem :git => 'git://github.com/carsonmcdonald/mruby-jpeg.git', :branch => 'master'

end
```

or

In the mruby directory:

```bash
mkdir mrbgems
cd mrbgems
git clone git://github.com/carsonmcdonald/mruby-jpeg.git
```

Then in your build config add:

```ruby
MRuby::Build.new do |conf|
  
  conf.gem 'mrbgems/mruby-jpeg'

end
```

## Example Use

```ruby
jpeg = JPEG.read("test.jpg")
# Raw jpeg data found in jpeg.data
```

## Options

You can pass the following options when reading a JPEG file:

  - :force_grayscale - Force the image to be read in grayscale

## License

MIT - See LICENSE
