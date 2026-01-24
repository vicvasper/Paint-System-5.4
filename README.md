# Paint-System-5.4

https://github.com/user-attachments/assets/6cd6eef6-3041-427f-9112-82b9a04e6600

# Paint System

Runtime material painting tool for Unreal Engine 5.4. Dynamically paint materials on Static and Skeletal Meshes with precise control over RGB and alpha channels. Fully integrated with Blueprint for rapid prototyping and gameplay mechanics.

## Features

### Runtime Material Painting
- Paint directly on mesh surfaces during gameplay or in editor
- Support for both Static Meshes and Skeletal Meshes
- Real-time visual feedback
- No pre-baked textures required

### Channel Control
- Independent RGB channel manipulation
- Alpha channel support for transparency and masking
- Additive and subtractive painting modes
- Configurable brush intensity and falloff

### Blueprint Integration
- Fully exposed to Blueprint for designer-friendly workflows
- Easy-to-use component system
- Event-driven architecture for paint interactions
- Customizable paint parameters at runtime

### Performance Optimized
- Efficient render target updates
- Minimal CPU overhead
- GPU-accelerated painting operations
- Scalable for different hardware configurations

## Requirements

- Unreal Engine 5.4 or higher
- C++ project (or converted from Blueprint project)
- RenderCore and RHI modules access
- Basic understanding of materials and render targets

## Installation

### Clone the Repository
```bash
git clone https://github.com/vicvasper/PaintSystem.git
```

### Option 1: Use as Standalone Project
1. Open `PaintSystem.uproject` directly in Unreal Engine 5.4
2. The project will compile automatically on first launch
3. Explore the demo content to see paint system in action

### Option 2: Integrate into Existing Project
1. Copy the Source/PaintSystem folder contents to your project's Source folder
2. Update your project's `.Build.cs` file to include dependencies:
   ```csharp
   PublicDependencyModuleNames.AddRange(new string[] { 
       "Core", "CoreUObject", "Engine", "InputCore", 
       "RenderCore", "RHI" 
   });
   ```
3. Regenerate project files
4. Build your project in Visual Studio or Rider

## Usage

### Basic Setup

#### 1. Create Paint Material
Create a material with a Render Target parameter to receive paint data:
- Add a Texture Sample parameter named "PaintTexture"
- Set texture type to Render Target 2D
- Connect to Base Color (or desired material input)

#### 2. Setup Paint Component (Blueprint)
Add the Paint System component to your actor:
1. Add PaintComponent to your Blueprint actor
2. Set target mesh component reference
3. Configure paint settings (brush size, color, intensity)
4. Call Paint function on input events

#### 3. Configure Mesh Material
Assign your paint-enabled material to the mesh:
- Set material instance with PaintTexture parameter
- Ensure proper UV mapping for paint coordinates

### Blueprint Example

```
// Component Setup
PaintComponent->SetTargetMesh(MeshComponent);
PaintComponent->SetBrushSize(10.0f);
PaintComponent->SetPaintColor(FLinearColor::Red);

// Paint on Mouse Click
OnMouseClicked()
{
    PaintComponent->PaintAtLocation(HitLocation, HitNormal);
}
```

### Advanced Features

#### Custom Brush Shapes
- Configure brush falloff curves
- Use texture-based brush stamps
- Implement custom blending modes

#### Multi-Channel Painting
Paint different data to RGBA channels:
- R: Base color tint
- G: Roughness/smoothness
- B: Metallic values
- A: Emission/mask data

#### Performance Optimization
- Batch paint operations for multiple strokes
- Use lower resolution render targets when appropriate
- Implement LOD-based paint detail

## Technical Details

### Architecture

**Core Components**
- `PaintComponent`: Main component handling paint logic
- `PaintData`: Stores paint configuration and state
- `RenderTargetManager`: Manages render target creation and updates
- `PaintShader`: GPU shader for paint application

### Paint Process Flow
1. Input detection (mouse/controller/VR)
2. Surface hit detection via raycast
3. UV coordinate calculation
4. Render target update via GPU shader
5. Material parameter update
6. Visual feedback

### Render Target Management
- Dynamic creation based on mesh UV layout
- Automatic resolution calculation
- Memory pooling for efficiency
- Persistent or temporary modes

### Shader Implementation
Paint operations executed on GPU for performance:
- Brush shape rendering
- Color blending
- Alpha compositing
- Multi-channel writing

## Configuration

### Paint Settings
- **Brush Size**: Radius in world units or UV space
- **Brush Intensity**: Paint opacity/strength (0-1)
- **Brush Falloff**: Edge softness curve
- **Blend Mode**: Add, Multiply, Replace, Erase
- **Color**: RGB values to paint
- **Channels**: Which RGBA channels to affect

### Material Parameters
Required material parameters for full functionality:
- `PaintTexture`: Render Target 2D
- `PaintIntensity`: Scalar (optional)
- `TintColor`: Linear Color (optional)

## Performance Considerations

### Optimization Tips
- Use appropriate render target resolution (512x512 to 2048x2048)
- Batch paint strokes when possible
- Limit active paint components in scene
- Use texture compression for final baked results
- Consider lower update frequency for distant objects

### Memory Usage
Typical memory per painted mesh:
- 512x512 RT: ~1 MB
- 1024x1024 RT: ~4 MB
- 2048x2048 RT: ~16 MB

Calculate based on: Width x Height x 4 bytes (RGBA)

## Use Cases

### Gameplay Mechanics
- Player graffiti/tagging systems
- Blood splatter and decals
- Environmental weathering
- Territory marking
- Paint-based puzzles

### Art Tools
- Runtime texture painting
- Procedural detail addition
- Quick material variations
- Prototyping asset variations

### VR Applications
- Virtual painting experiences
- 3D model customization
- Interactive art installations

## Troubleshooting

### Paint Not Appearing
- Verify mesh has valid UV coordinates
- Check material has PaintTexture parameter
- Ensure render target is assigned
- Confirm paint component is active

### Performance Issues
- Reduce render target resolution
- Limit number of simultaneously painted meshes
- Check for unnecessary paint updates
- Profile GPU usage with Unreal Insights

### Incorrect Paint Location
- Verify UV mapping is correct
- Check raycast hit detection
- Ensure proper world-to-UV coordinate conversion
- Test with simple UV debug material

## Known Limitations

- Requires valid UV0 channel on mesh
- Paint data not automatically serialized (requires custom save system)
- Maximum resolution limited by GPU memory
- Real-time painting may impact frame rate on lower-end hardware

## Roadmap

### Planned Features
- Save/load paint data to disk
- Multi-layer painting support
- Undo/redo functionality
- Networked multiplayer painting
- Mobile platform optimization
- Brush preset system

## Examples and Demos

The project includes example content demonstrating:
- Basic paint setup on Static Mesh
- Skeletal Mesh painting
- Multi-channel painting
- Custom brush shapes
- VR painting implementation

Check the demo video in the repository for visual reference.

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Test thoroughly on UE 5.4
4. Submit a pull request with detailed description

## License

MIT License - See LICENSE file for details

## Author

Created by Victor Rivas ([@vicvasper](https://github.com/vicvasper))

For questions, bug reports, or feature requests, please open an issue on GitHub.

## Links

- Repository: [github.com/vicvasper/PaintSystem](https://github.com/vicvasper/Paint-System-5.4)
- Portfolio: [vicvasper.github.io/README](https://vicvasper.github.io/README/)
- LinkedIn: [linkedin.com/in/victorrivasperez](https://www.linkedin.com/in/victorrivasperez/)

## Acknowledgments

Built with Unreal Engine 5.4. Special thanks to the Unreal Engine community for shader and rendering insights.

