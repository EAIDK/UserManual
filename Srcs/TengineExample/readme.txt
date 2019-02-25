# Build and run Tengine example on EAIDK

1. Build example

-------------------------------------------------------------------------------
cmake .
make
-------------------------------------------------------------------------------

2. Prepare tengine model files
Generate Tengine model file by using Tengine convert tool, and put the Tengine model file
in the example folder.

3. Run example
Run 'tm_classify' on EAIDK platform.
-------------------------------------------------------------------------------
sudo ./tm_classify -t sqz.tmfile -l synset_words.txt -i cat.jpg
-------------------------------------------------------------------------------

The usage of Classify is as below:
-------------------------------------------------------------------------------
[Usage]: ./tm_classify [-n model_name] [-t tm_file] [-l label_file] [-i image_file]
                       [-g img_h,img_w] [-s scale] [-w mean[0],mean[1],mean[2]] [-r repeat_count] [-h]

    Default model name is : `squeezenet`
    Default label file is : `Tengine_ROOT/models/synset_words.txt`
    Default image file is : `Tengine_ROOT/tests/images/cat.jpg`
    Default image size is : `227,227`
    Default scale value is : `1.f`
    Default mean values are : `104.007,116.669,122.679`
-------------------------------------------------------------------------------
