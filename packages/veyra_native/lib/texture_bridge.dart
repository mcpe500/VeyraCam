class TextureBridge {
  final int sharedTextureHandle;
  final int width;
  final int height;

  const TextureBridge({
    required this.sharedTextureHandle,
    this.width = 1280,
    this.height = 720,
  });

  bool get isValid => sharedTextureHandle != 0;
}
