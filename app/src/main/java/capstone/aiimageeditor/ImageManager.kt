package capstone.aiimageeditor

import android.app.Application
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.ImageDecoder
import android.net.Uri
import android.os.AsyncTask
import android.os.Build
import android.provider.MediaStore
import android.util.Log
import org.opencv.android.Utils
import org.opencv.core.Mat

class ImageManager : Application() {
    lateinit var original:Bitmap
    lateinit var mask:Bitmap
    lateinit var person:Bitmap
    lateinit var background:Bitmap
    private lateinit var listener:OnFinishInpaint
    var isInpainting=false

     fun getImageFromUri(selectedPhotoUri: Uri): Bitmap {
        return if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            MediaStore.Images.Media.getBitmap(this.contentResolver, selectedPhotoUri)
        } else {
            val source = ImageDecoder.createSource(this.contentResolver, selectedPhotoUri)
            ImageDecoder.decodeBitmap(source){decoder,_,_->decoder.isMutableRequired=true}
        }
    }

    fun setOnFinishInpaint(listener:OnFinishInpaint){
        this.listener=listener
    }

    fun startInpaint(){
        background = Bitmap.createBitmap(original)
        InpaintTask().execute(0)
    }

    fun resetImages(){
        original = Bitmap.createBitmap(1,1,Bitmap.Config.ARGB_8888)
        mask = Bitmap.createBitmap(1,1,Bitmap.Config.ARGB_8888)
        person=Bitmap.createBitmap(1,1,Bitmap.Config.ARGB_8888)
        background=Bitmap.createBitmap(1,1,Bitmap.Config.ARGB_8888)
    }

    fun loadOriginal(uri:Uri) {
        original = getImageFromUri(uri)
    }

    fun mergeImage():Bitmap{
        val bitmap = Bitmap.createBitmap(background)
        val canvas = Canvas(bitmap)
        canvas.drawBitmap(person,0f,0f,null)
        return bitmap
    }

    interface OnFinishInpaint{
        fun onFinishInpaint()
    }

    inner class InpaintTask: AsyncTask<Int, Int, Bitmap>() {
        override fun doInBackground(vararg p0: Int?): Bitmap {
            isInpainting=true
            val source= Mat()
            val mask = Mat()
            Utils.bitmapToMat(original,source)
            Utils.bitmapToMat(this@ImageManager.mask,mask)
            startInpaint(source.nativeObjAddr,mask.nativeObjAddr)
            val bitmap = Bitmap.createBitmap(original.width,original.height,Bitmap.Config.ARGB_8888)
            Utils.matToBitmap(source,bitmap)
            return bitmap
        }

        override fun onPostExecute(result: Bitmap?) {
            super.onPostExecute(result)
            if(result!=null){
                background=result
                isInpainting=false
                Log.i("!!","inpaint finished")
                listener.onFinishInpaint()
            }
        }

        external fun startInpaint(image: Long, mask: Long)
    }
}