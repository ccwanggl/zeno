#include "zeno/zeno.h"
#include "zeno/types/MaterialObject.h"
#include "zeno/types/PrimitiveObject.h"
#include "zeno/types/ListObject.h"
#include "zeno/types/StringObject.h"
#include <zeno/types/UserData.h>
#include <tinygltf/json.hpp>
namespace zeno
{
  /*struct MakeMaterial
      : zeno::INode
  {
    virtual void apply() override
    {
      auto vert = get_input<zeno::StringObject>("vert")->get();
      auto frag = get_input<zeno::StringObject>("frag")->get();
      auto common = get_input<zeno::StringObject>("common")->get();
      auto extensions = get_input<zeno::StringObject>("extensions")->get();
      auto mtl = std::make_shared<zeno::MaterialObject>();

      if (vert.empty()) vert = R"(
#version 120

uniform mat4 mVP;
uniform mat4 mInvVP;
uniform mat4 mView;
uniform mat4 mProj;
uniform mat4 mInvView;
uniform mat4 mInvProj;

attribute vec3 vPosition;
attribute vec3 vColor;
attribute vec3 vNormal;

varying vec3 position;
varying vec3 iColor;
varying vec3 iNormal;

void main()
{
  position = vPosition;
  iColor = vColor;
  iNormal = vNormal;

  gl_Position = mVP * vec4(position, 1.0);
}
)";

      if (frag.empty()) frag = R"(
#version 120

uniform mat4 mVP;
uniform mat4 mInvVP;
uniform mat4 mView;
uniform mat4 mProj;
uniform mat4 mInvView;
uniform mat4 mInvProj;
uniform bool mSmoothShading;
uniform bool mRenderWireframe;

varying vec3 position;
varying vec3 iColor;
varying vec3 iNormal;

void main()
{
  gl_FragColor = vec4(8.0, 0.0, 0.0, 1.0);
}
)";

      mtl->vert = vert;
      mtl->frag = frag;
      mtl->common = common;
      mtl->extensions = extensions;
      set_output("mtl", std::move(mtl));
    }
  };

  ZENDEFNODE(
      MakeMaterial,
      {
          {
              {"string", "vert", ""},
              {"string", "frag", ""},
              {"string", "common", ""},
              {"string", "extensions", ""},
          },
          {
              {"material", "mtl"},
          },
          {},
          {
              "shader",
          },
      });*/

struct ExtractMaterialShader : zeno::INode
{
    virtual void apply() override {
      auto mtl = get_input<zeno::MaterialObject>("mtl");
      auto s = [] (std::string const &s) { auto p = std::make_shared<StringObject>(); p->set(s); return p; };
      set_output("vert", s(mtl->vert));
      set_output("frag", s(mtl->frag));
      set_output("common", s(mtl->common));
      set_output("extensions", s(mtl->extensions));
    }
};

  struct SetMaterial
      : zeno::INode
  {
    virtual void apply() override
    {
      auto prim = get_input<zeno::PrimitiveObject>("prim");
      auto mtl = get_input<zeno::MaterialObject>("mtl");
      prim->mtl = mtl;
      set_output("prim", std::move(prim));
    }
  };

  ZENDEFNODE(
      SetMaterial,
      {
          {
              {"primitive", "prim"},
              {"material", "mtl"},
          },
          {
              {"primitive", "prim"},
          },
          {},
          {
              "deprecated",
          },
      });

  struct BindMaterial
      : zeno::INode
  {
    virtual void apply() override
    {
        auto mtlid = get_input2<std::string>("mtlid");

        if( has_input2<zeno::ListObject>("object") ) {
            auto list = get_input2<zeno::ListObject>("object");
            for (auto p: list->arr) {
                auto np = std::dynamic_pointer_cast<PrimitiveObject>(p);
                np->userData().setLiterial("mtlid", std::move(mtlid));
            }
            set_output("object", std::move(list));
            return;
        }

      auto obj = get_input<zeno::PrimitiveObject>("object");
      
      auto mtlid2 = get_input2<std::string>("mtlid");
      int matNum = obj->userData().get2<int>("matNum",0);
      for(int i=0; i<matNum; i++)
      {
          auto key = "Material_" + to_string(i);
          obj->userData().erase(key);
      }
      obj->userData().set2("matNum", 1);
      obj->userData().setLiterial("Material_0", std::move(mtlid2));
      obj->userData().setLiterial("mtlid", std::move(mtlid));
      if(obj->tris.size()>0)
      {
          obj->tris.add_attr<int>("matid");
          obj->tris.attr<int>("matid").assign(obj->tris.size(),0);
      }
      if(obj->quads.size()>0)
      {
          obj->quads.add_attr<int>("matid");
          obj->quads.attr<int>("matid").assign(obj->quads.size(),0);
      }
      if(obj->polys.size()>0)
      {
          obj->polys.add_attr<int>("matid");
          obj->polys.attr<int>("matid").assign(obj->polys.size(),0);
      }
      set_output("object", std::move(obj));
    }
  };

  ZENDEFNODE(
      BindMaterial,
      {
          {
              {"object"},
              {"string", "mtlid", "Mat1"},
          },
          {
              {"object"},
          },
          {},
          {
              "shader",
          },
      });

    struct BindLight
        : zeno::INode
    {
        virtual void apply() override
        {
            auto obj = get_input<zeno::IObject>("object");
            auto isL = get_input2<int>("islight");
            auto inverdir = get_input2<int>("invertdir");

            auto prim = dynamic_cast<zeno::PrimitiveObject *>(obj.get());

            zeno::vec3f clr;
            if (! prim->verts.has_attr("clr")) {
                auto &clr = prim->verts.add_attr<zeno::vec3f>("clr");
                clr[0] = zeno::vec3f(30000.0f, 30000.0f, 30000.0f);
            }

            if(inverdir){
                for(int i=0;i<prim->tris.size(); i++){
                    int tmp = prim->tris[i][2];
                    prim->tris[i][2] = prim->tris[i][0];
                    prim->tris[i][0] = tmp;
                }
            }

            obj->userData().set2("isRealTimeObject", std::move(isL));
            obj->userData().set2("isL", std::move(isL));
            obj->userData().set2("ivD", std::move(inverdir));
            set_output("object", std::move(obj));
        }
    };

    ZENDEFNODE(
        BindLight,
        {
            {
                {"object"},
                {"bool", "islight", "1"},// actually string or list
                {"bool", "invertdir", "0"}
            },
            {
                {"object"},
            },
            {},
            {
                "shader",
            },
        });

    struct PrimAttrAsShaderBuffer : zeno::INode {

        static std::string aKey() {
            return "attrNames";
        }
        static std::string bKey() { 
            return "bindNames";
        }

        virtual void apply() override {

            auto prim = get_input2<zeno::PrimitiveObject>("in");

            auto a_value = get_input2<std::string>(aKey(), "");
            auto b_value = get_input2<std::string>(bKey(), "");

            auto task = [](const std::string& raw){
                
                std::string segment;
                std::stringstream test(raw);
                std::vector<std::string> result;

                while(std::getline(test, segment, ','))
                {
                    result.push_back(segment);
                }
                return result;
            };

            std::vector<std::string> a_list = task(a_value);
            std::vector<std::string> b_list = task(b_value);

            if (a_list.size() != b_list.size()) {
                throw std::runtime_error("buffer count doesn't match attr count");
            }

            nlohmann::json json;

            for (size_t i=0; i<a_list.size(); ++i) {
                auto a = a_list[i];
                auto b = b_list[i];
                json[a] = b;
            }

            prim->userData().set2("ShaderAttributes", json.dump());
            set_output("out", std::move(prim));
        }
    };

    ZENDEFNODE( PrimAttrAsShaderBuffer,
    {
        {
            {"in"},
            {"string", PrimAttrAsShaderBuffer::aKey(), ""},
            {"string", PrimAttrAsShaderBuffer::bKey(), ""}
        },
        {
            {"out"},
        },
        {},
        {
            "shader",
        },
    });

} // namespace zeno
