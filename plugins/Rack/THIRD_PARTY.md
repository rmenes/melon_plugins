# Melon Rack — third-party software and credits

Melon Rack links the libraries below. Their licences allow use in free and commercial software, as long as the copyright notices and licence texts are kept with the source and with the distributed plugins. The exact copyright lines are in each project's `LICENSE` file (CMake downloads them to `build/_deps/neuralaudio-src/` and its `deps/` folder).

| Project | Author | Role in Melon Rack | Licence |
|---|---|---|---|
| [NeuralRack](https://github.com/brummer10/NeuralRack) | Hermann Meyer (brummer10) | Design reference: chain, controls, resampling approach. No code is copied. | BSD 3-Clause |
| [NeuralAudio](https://github.com/mikeoliphant/NeuralAudio) | Mike Oliphant | Neural model engine (NAM WaveNet/LSTM, AIDA-X) | MIT |
| [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore) | Steven Atkinson | NAM model format, used through NeuralAudio | MIT |
| [RTNeural](https://github.com/jatinchowdhury18/RTNeural) | Jatin Chowdhury | AIDA-X / Keras models, used through NeuralAudio | BSD 3-Clause |
| [Eigen](https://eigen.tuxfamily.org) | Eigen contributors | Linear algebra, used by the engines | MPL 2.0 |
| [JUCE](https://juce.com) | Raw Material Software | Plugin framework (the whole series) | AGPLv3 or commercial |
| [TONE3000](https://www.tone3000.com) API | TONE3000 | Sign-in and user-requested downloads of NAM captures and IRs. No tones are distributed with Melon Rack. | [API Terms of Service](https://www.tone3000.com/api/terms) |

## MIT licence (NeuralAudio, NeuralAmpModelerCore)

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## BSD 3-Clause licence (RTNeural; also NeuralRack's licence)

```
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
