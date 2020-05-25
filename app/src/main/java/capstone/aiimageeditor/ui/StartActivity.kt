package capstone.aiimageeditor.ui

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle

import android.provider.MediaStore
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import capstone.aiimageeditor.ImageManager
import capstone.aiimageeditor.R
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import com.gun0912.tedpermission.PermissionListener
import com.gun0912.tedpermission.TedPermission
import com.itaewonproject.adapter.AdapterImageList
import java.util.*


class StartActivity : AppCompatActivity() {

    private lateinit var imgList:RecyclerView
    var images = ArrayList<String>()
    private lateinit var adapter:AdapterImageList
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_start)
        tedPermission()



        imgList = findViewById(R.id.recyclerView)

        if(getSavedStringSets()!=null) images = getSavedStringSets()!!
        adapter = AdapterImageList(this, images)
        imgList.adapter = adapter
        adapter.setOnItemClickListener(object: AdapterImageList.OnItemClickListener{
            override fun onClick(position: Int) {
                if(position==0){
                    val intent = Intent(Intent.ACTION_PICK)
                    intent.setType(MediaStore.Images.Media.CONTENT_TYPE)
                    startActivityForResult(intent,
                        PICK_FROM_ALBUM
                    )
                }else
                {
                    val str = images[position-1]
                    images.remove(str)
                    images.add(0,str)
                    gotoNextActivity(Uri.parse(str))
                }

            }
        })

        val linearLayoutManager = LinearLayoutManager(this, RecyclerView.HORIZONTAL, false)
        imgList.layoutManager = linearLayoutManager
        imgList.setHasFixedSize(true)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if(requestCode== PICK_FROM_ALBUM && data!=null){
            val photoUri = data.data as Uri
            images.add(0,photoUri.toString())
            if(images.size>10) images.removeAt(images.lastIndex)
            gotoNextActivity(photoUri)
        }
        super.onActivityResult(requestCode, resultCode, data)
    }

    private fun gotoNextActivity(uri:Uri){
        val intent = Intent(this, MainActivity::class.java)
        //intent.putExtra("photo",uri)
/*Glide.with(applicationContext).asBitmap().load(uri).into(object: CustomTarget<Bitmap>(){
override fun onLoadCleared(placeholder: Drawable?) {
TODO("Not yet implemented")
}

override fun onResourceReady(resource: Bitmap, transition: Transition<in Bitmap>?) {
(application as ImageManager).original=resource
}

})*/
        (application as ImageManager).loadOriginal(uri)
        saveStringSet(images)
        startActivity(intent)
    }



    override fun onResume() {
        super.onResume()
        adapter.notifyDataSetChanged()
    }

    private fun tedPermission(){
        val permissionListener = object: PermissionListener {
            override fun onPermissionGranted() {

            }

            override fun onPermissionDenied(deniedPermissions: ArrayList<String>?) {
                Toast.makeText(this@StartActivity, "서비스 이용을 위해 권한을 허용해주세요.",Toast.LENGTH_LONG)

            }

        }

        TedPermission.with(this)
            .setPermissionListener(permissionListener)
            .setRationaleMessage(R.string.permission_desc)
            .setDeniedMessage(R.string.permission_rej)
            .setPermissions(android.Manifest.permission.READ_EXTERNAL_STORAGE,android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
            .check()


    }

    companion object {
        // Used to load the 'native-lib' library on application startup.
        val PICK_FROM_ALBUM=1
        private val PREF_STRING_SET_KEY = "URI"
        init {
            System.loadLibrary("native-lib")
        }
    }

    fun saveStringSet( mList: ArrayList<String>) {
        val sp: SharedPreferences = getSharedPreferences("uri", Context.MODE_PRIVATE)//PreferenceManager.getDefaultSharedPreferences(context)
        val editor: SharedPreferences.Editor = sp.edit()
        editor.putString(PREF_STRING_SET_KEY, Gson().toJson(mList))
        editor.apply()
    }

    fun getSavedStringSets(): ArrayList<String>? {
        val sp = getSharedPreferences("uri", Context.MODE_PRIVATE)
        return Gson().fromJson(sp.getString(PREF_STRING_SET_KEY, null),
            TypeToken.getParameterized(ArrayList::class.java, String::class.java).type)
    }

}