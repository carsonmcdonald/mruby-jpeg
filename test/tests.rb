assert('JPEG decompress file') do
  jpg = JPEG::decompress_file(TEST_ARGS['test_jpeg'])
  jpg.height == 3000 && jpg.width == 3000
end
